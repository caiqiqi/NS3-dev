/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

// Network topology
//
//      
//
//       ----------   ---------- 
// n0 -- | Switch | --| Switch | -- n3
//       ----------   ----------
//           |            |
//           n1           n2
//
//
//	and port number corresponds to node number, so port 0 is connected to n0, for example.

#include <iostream>
#include <fstream>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/openflow-module.h"
#include "ns3/log.h"
#include "ns3/bridge-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("OpenFlowCsmaSwitch");

// If this value is false,then by default you won't see  verbose output in the terminal,
// unless you specify the -v option
bool verbose = true;
bool use_drop = false;
ns3::Time timeout = ns3::Seconds (0);

bool
SetVerbose (std::string value)
{
  verbose = true;
  return true;
}

bool
SetDrop (std::string value)
{
  use_drop = true;
  return true;
}

bool
SetTimeout (std::string value)
{
  try {
      timeout = ns3::Seconds (atof (value.c_str ()));
      return true;
    }
  catch (...) { return false; }
  return false;
}

int
main (int argc, char *argv[])
{
  #ifdef NS3_OPENFLOW
  //
  // Allow the user to override any of the defaults and the above Bind() at
  // run-time, via command-line arguments
  //
  CommandLine cmd;
  cmd.AddValue ("v", "Verbose (turns on logging).", MakeCallback (&SetVerbose));
  cmd.AddValue ("verbose", "Verbose (turns on logging).", MakeCallback (&SetVerbose));
  cmd.AddValue ("d", "Use Drop Controller (Learning if not specified).", MakeCallback (&SetDrop));
  cmd.AddValue ("drop", "Use Drop Controller (Learning if not specified).", MakeCallback (&SetDrop));
  cmd.AddValue ("t", "Learning Controller Timeout (has no effect if drop controller is specified).", MakeCallback ( &SetTimeout));
  cmd.AddValue ("timeout", "Learning Controller Timeout (has no effect if drop controller is specified).", MakeCallback ( &SetTimeout));

  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("OpenFlowCsmaSwitch", LOG_LEVEL_INFO);
      LogComponentEnable ("OpenFlowInterface", LOG_LEVEL_INFO);
      LogComponentEnable ("OpenFlowSwitchNetDevice", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);  
      LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO); 
    }

  //
  // Explicitly create the nodes required by the topology (shown above).
  //
  NS_LOG_INFO ("Create nodes.");
  NodeContainer terminals;
  terminals.Create (4);

  NodeContainer ofSwitch;
  ofSwitch.Create (2);

  NS_LOG_INFO ("Build Topology");
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", DataRateValue (5000000));   // 5M bandwidth
  csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));   // 2ms delay

  // Create the csma links, from each terminal to the switch
  NetDeviceContainer terminalDevices;
  NetDeviceContainer switchDevice1, switchDevice2;
  NetDeviceContainer link;

  //Connect ofSwitch1 to ofSwitch2  
  link = csma.Install(NodeContainer(ofSwitch.Get(0),ofSwitch.Get(1)));  
  switchDevice1.Add(link.Get(0));  
  switchDevice2.Add(link.Get(1));
  
  //Connect terminal1 and terminal2 to ofSwitch1  
  for (int i = 0;i < 2;i ++)  
  {  
      link = csma.Install(NodeContainer(terminals.Get(i),ofSwitch.Get(0)));  
      terminalDevices.Add(link.Get(0));  
      switchDevice1.Add(link.Get(1));  
  }  
  
  //Connect terminal3 and terminal4 to ofSwitch2  
  for (int i = 2;i < 4;i ++)  
  {  
      link = csma.Install(NodeContainer(terminals.Get(i),ofSwitch.Get(1)));  
      terminalDevices.Add(link.Get(0));  
      switchDevice2.Add(link.Get(1));  
  }


  //Create the switch netdevice,which will do the packet switching
  Ptr<Node> switchNode1 = ofSwitch.Get (0);
  Ptr<Node> switchNode2 = ofSwitch.Get (1);
  
  OpenFlowSwitchHelper switchHelper;

  if (use_drop)
    {
      Ptr<ns3::ofi::DropController> controller1 = CreateObject<ns3::ofi::DropController> ();
      switchHelper.Install (switchNode1, switchDevice1, controller1);
      Ptr<ns3::ofi::DropController> controller2 = CreateObject<ns3::ofi::DropController> ();
      switchHelper.Install (switchNode2, switchDevice2, controller2);
    }
  else
    {
      Ptr<ns3::ofi::LearningController> controller1 = CreateObject<ns3::ofi::LearningController> ();
      if (!timeout.IsZero ()) controller1->SetAttribute ("ExpirationTime", TimeValue (timeout));
      switchHelper.Install (switchNode1, switchDevice1, controller1);
      Ptr<ns3::ofi::LearningController> controller2 = CreateObject<ns3::ofi::LearningController> ();
      if (!timeout.IsZero ()) controller2->SetAttribute ("ExpirationTime", TimeValue (timeout));
      switchHelper.Install (switchNode2, switchDevice2, controller2);
    }

  // Add internet stack to the terminals
  InternetStackHelper internet;
  internet.Install (terminals);

  // We've got the "hardware" in place.  Now we need to add IP addresses.
  NS_LOG_INFO ("Assign IP Addresses.");
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interface = ipv4.Assign (terminalDevices);

  // Add applications
  NS_LOG_INFO ("Create Applications.");
  uint16_t port = 9;   // Discard port (RFC 863)
  UdpEchoServerHelper echoServer (port);  
  ApplicationContainer serverApps = echoServer.Install (terminals.Get(0));  
  serverApps.Start (Seconds(1.0));  
  serverApps.Stop (Seconds(10.0));  
  
  UdpEchoClientHelper echoClient (Ipv4Address("10.1.1.1"),port);  
  echoClient.SetAttribute ("MaxPackets", UintegerValue (5));  
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));  
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));  
  ApplicationContainer clientApps = echoClient.Install(terminals.Get(3));  
  clientApps.Start (Seconds(2.0));  
  clientApps.Stop (Seconds(10.0));
  

  NS_LOG_INFO ("Configure Tracing.");

  //
  // Configure tracing of all enqueue, dequeue, and NetDevice receive events.
  // Trace output will be sent to the file as below
  //
  AsciiTraceHelper ascii;
  csma.EnableAsciiAll (ascii.CreateFileStream ("openflow-2-switches.tr"));

  //
  // Also configure some tcpdump traces; each interface will be traced.
  // The output files will be named:
  //     openflow-switch-<nodeId>-<interfaceId>.pcap
  // and can be read by the "tcpdump -r" command (use "-tt" option to
  // display timestamps correctly)
  //
  csma.EnablePcapAll ("openflow-2-switches", false);

  //
  // Now, do the actual simulation.
  //
  NS_LOG_INFO ("Run Simulation.");
  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");
  #else
  NS_LOG_INFO ("NS-3 OpenFlow is not enabled. Cannot run simulation.");
  #endif // NS3_OPENFLOW
}
