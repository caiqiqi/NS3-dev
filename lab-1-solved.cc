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



/*
	LAB Assignment #1
	1. Create a simple topology of two nodes (Node1, Node2)
	separated by a point-to-point link.

	2. Setup a UdpClient on one Node1 and a UdpServer on Node2.
	Let it be of a fixed data rate Rate1.

	3. Start the client application, and measure end to end throughput
	whilst varying the latency of the link.

	4. Now add another client application to Node1 and a server instance to Node2.
	What do you need to configure to ensure that there is no conflict?

	5. Repeat step 3 with the extra client and server application instances.
	Show screenshots of pcap traces which indicate that delivery is made to the appropriate server instance.

	Solution by: Konstantinos Katsaros (K.Katsaros@surrey.ac.uk)
	based on udp-client-server.cc
*/

// Network topology
//
//       n1 ------ n2
//            p2p
//
// - UDP flows from n1 to n2

#include <fstream>
#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Lab1");


int main (int argc, char *argv[])
{
  double lat = 2.0;
  uint64_t rate = 5000000; // Data rate in bps
  double interval = 0.05;

  CommandLine cmd;
  cmd.AddValue ("latency", "P2P link Latency in miliseconds", lat);
  cmd.AddValue ("rate", "P2P data rate in bps", rate);
  cmd.AddValue ("interval", "UDP client packet interval", interval);

  cmd.Parse (argc, argv);

//
// Enable logging for UdpClient and UdpServer
//
  LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
  //LogComponentEnable ("UdpClient", LOG_LEVEL_FUNCTION);
  LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);
  //LogComponentEnable ("UdpServer", LOG_LEVEL_FUNCTION);

//
// Explicitly create the nodes required by the topology (shown above).
//
  NS_LOG_INFO ("Create nodes.");
  NodeContainer n;
  n.Create (3);

  NS_LOG_INFO ("Create channels.");
//
// Explicitly create the channels required by the topology (shown above).
//
  PointToPointHelper p2p;
  p2p.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (lat)));
  p2p.SetDeviceAttribute ("DataRate", DataRateValue (DataRate (rate)));
  p2p.SetDeviceAttribute ("Mtu", UintegerValue (1400));  
  // MTU: Maximum Transmission Unit.
  // 指一种通信协议某一层上所能通过的最大数据包大小(以字节为单位)。
  NetDeviceContainer dev = p2p.Install (n.Get(0), n.Get(1));
  NetDeviceContainer dev2 = p2p.Install (n.Get(1), n.Get(2));

//
// We've got the "hardware" in place.  Now we need to add IP addresses.
//

//
// Install Internet Stack
//
  InternetStackHelper internet;
  internet.Install (n);
  Ipv4AddressHelper ipv4;

  NS_LOG_INFO ("Assign IP Addresses.");
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipv4.Assign (dev);

  ipv4.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i2 = ipv4.Assign (dev2);


  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  NS_LOG_INFO ("Create Applications.");
//
// Create one udpServer application on node one.
//
  uint16_t port1 = 8000; // Need different port numbers to ensure there is no conflict
  uint16_t port2 = 8001;
  UdpServerHelper server1 (port1);
  UdpServerHelper server2 (port2);
  ApplicationContainer apps;
  apps = server1.Install (n.Get (2));
  apps = server2.Install (n.Get (2));

  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (10.0));

//
// Create one UdpClient application to send UDP datagrams from node zero to
// node one.
//
  uint32_t MaxPacketSize = 1024;
  Time interPacketInterval = Seconds (interval);
  uint32_t maxPacketCount = 320;
  UdpClientHelper client1 (i2.GetAddress (1), port1);
  UdpClientHelper client2 (i2.GetAddress (1), port2);

  client1.SetAttribute ("MaxPackets", UintegerValue (maxPacketCount));  // 最大数据包大小
  client1.SetAttribute ("Interval", TimeValue (interPacketInterval));   // 时间间隔
  client1.SetAttribute ("PacketSize", UintegerValue (MaxPacketSize));   // 包大小

  client2.SetAttribute ("MaxPackets", UintegerValue (maxPacketCount));
  client2.SetAttribute ("Interval", TimeValue (interPacketInterval));
  client2.SetAttribute ("PacketSize", UintegerValue (MaxPacketSize));

  apps = client1.Install (n.Get (0));
  apps = client2.Install (n.Get (0));

  apps.Start (Seconds (2.0));
  apps.Stop (Seconds (10.0));


//
// Tracing
//
  AsciiTraceHelper ascii;
  p2p.EnableAscii(ascii.CreateFileStream ("lab-1.tr"), dev);
  p2p.EnablePcap("lab-1", dev, false);

/*
** Calculate Throughput using Flowmonitor
*/
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();


/*
** Now, do the actual simulation.
*/
  NS_LOG_INFO ("Run Simulation.");
  Simulator::Stop (Seconds(11.0));
  Simulator::Run ();

  monitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
	  /* `Ipv4FlowClassifier`
    Classifies packets by looking at their IP and TCP/UDP headers. 
    FiveTuple五元组是：(source-ip, destination-ip, protocol, source-port, destination-port)
    */

    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      if ((t.sourceAddress=="10.1.1.1" && t.destinationAddress == "10.1.2.2"))
      {
          std::cout << "Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
          std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";   // 传输了多少字节
          std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";   // 收到了多少字节
      	  std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds())/1024/1024  << " Mbps\n";
      }
     }



  monitor->SerializeToXmlFile("lab-1.flowmon", true, true);

  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");
}
