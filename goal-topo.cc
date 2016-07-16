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
//           ----Controller---
//            |             |
//         ----------     ---------- 
//  AP3 -- | Switch1 | --| Switch2 | -- H1
//         ----------     ----------
//   |      |       |          |
//   X     AP1     AP2         H2
//        | | |   | |||
//        X X |   X X|X
//            |      |
//            m2     m1  
//
// reference: http://blog.csdn.net/u012174021/article/details/42320033

#include <iostream>
#include <fstream>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/applications-module.h"
#include "ns3/openflow-module.h"
#include "ns3/log.h"
#include "ns3/bridge-helper.h"


#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("GoalTopoScript");

// If this value is false,then by default you won't see  verbose output in the terminal,
// unless you specify the -v option
bool verbose = false;
bool use_drop = false;
bool tracing  = true;
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
  uint32_t nAp         = 3;
  uint32_t nSwitch     = 2;
  uint32_t nTerminal   = 2;
  uint32_t nAp1Station = 3;
  uint32_t nAp2Station = 4;
  uint32_t nAp3Station = 1;

  #ifdef NS3_OPENFLOW
  //
  // Allow the user to override any of the defaults and the above Bind() at
  // run-time, via command-line arguments
  //
  CommandLine cmd;
  cmd.AddValue ("nAp1Station", "Number of wifi STA devices of AP1", nAp1Station);
  cmd.AddValue ("nAp2Station", "Number of wifi STA devices of AP2", nAp2Station);
  cmd.AddValue ("nAp3Station", "Number of wifi STA devices of AP3", nAp3Station);

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
  NS_LOG_INFO ("-----Creating nodes-----");
 
  NodeContainer terminalsNode;
  terminalsNode.Create (nTerminal);   //2 Nodes(H1 and H2)-----node 0,1

  NodeContainer switchesNode;
  switchesNode.Create (nSwitch);    //2 Nodes(Switch1 and Switch2)-----node 2,3

  NodeContainer wifiApsNode;
  wifiApsNode.Create (nAp);    //3 Nodes(Ap1 Ap2 and Ap3)-----node 4,5,6
  
  // Creating every  Ap's stations
  NodeContainer wifiAp1Node = wifiApsNode.Get(0);
  NodeContainer wifiAp1StaNodes;
  wifiAp1StaNodes.Create(nAp1Station);    // node 7,8,9

  NodeContainer wifiAp2Node = wifiApsNode.Get(1);
  NodeContainer wifiAp2StaNodes;
  wifiAp2StaNodes.Create(nAp2Station);    // node 10,11,12,13

  NodeContainer wifiAp3Node = wifiApsNode.Get(2);
  NodeContainer wifiAp3StaNodes;
  wifiAp3StaNodes.Create(nAp3Station);    //  node 14

  // The next bit of code constructs the wifi device and the interconnection channel between these wifi nodes.
  // First, we configure the PHY and channel helpers:
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default();

  /* For simplicity, this code uses the default PHY layer configuration and channel models which are documented in
     the API doxygen documentation for the `YansWifiChannelHelper::Default` and `YansWifiPhyHelper::Default()`
     methods. Once these objects are created, we create a chennel object and associate it to our PHY layer manager
     to make sure that all the PHY layer objects created by the `YansWifiPhyHelper` share the same underlying
     channel, that is , they "share the same wireless medium and can communicate and interface": 
  */
  // 程序为了简单，直接创建默认的Channel和PHY。然后再将所有的PHY与Channel联系起来，保证其共享同样的无线媒介。
  phy.SetChannel (channel.Create() );
  /* Once the PHY helper is configured, we can focus on the MAC layer. Here we choose to work with "non-Qos MACs"
     so we use a NqosWifiMacHelper object to set MAC parameters.
  */
  WifiHelper wifi; // static ns3::WifiHelper ns3::WifiHelper::Default()’ is deprecated
  wifi.SetRemoteStationManager ("ns3::AarfWifiManager");
  // SetRemoteStationManager其实是设置了速率控制算法。这里的速率在tutorial中也没有说清楚，我觉得应该是wifi信道的数据传播速率。

  NqosWifiMacHelper mac = NqosWifiMacHelper::Default();
  /* The SetRemoteStationManager method tells the helper the type of rate control algorithm to use. Here, it is 
     asking the helper to use the "AARF algorithm" — details are, of course, available in Doxygen.
  */
  /* 
     by the helper is specified by `Attribute` as being of the “ns3::StaWifiMac” type. The use of `NqosWifiMacHelper` will 
     ensure that the “QosSupported” `Attribute` for created MAC objects is set false. The combination of these two configurations
     means that the MAC instance next created will be a non-QoS non-AP station (STA) in an infrastructure BSS (i.e., a BSS with an AP). 
     Finally, the “ActiveProbing” `Attribute` is set to false. This means that probe requests will not be sent by MACs created by this helper.
  */

  // Once all the station-specific parameters are fully configured, both at the MAC and PHY layers, 
  // we can invoke our now-familiar `Install` method to create the wifi devices of these stations:
  
  //------- Network AP1-------
  NetDeviceContainer stas1Device, ap1Device;    // station devices in AP1 network, and the AP1 itself
  Ssid ssid1 = Ssid ("ssid-AP1");
  // We want to make sure that our stations don't perform active probing.
  mac.SetType ("ns3::StaWifiMac", "Ssid", SsidValue (ssid1), "ActiveProbing", BooleanValue (false));
  stas1Device = wifi.Install(phy, mac, wifiAp1StaNodes );
  mac.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssid1));
  ap1Device   = wifi.Install(phy, mac, wifiAp1Node);

  //------- Network AP2-------
  NetDeviceContainer stas2Device, ap2Device;    // station devices in AP2 network, and the AP2 itself
  Ssid ssid2 = Ssid ("ssid-AP2");
  // We want to make sure that our stations don't perform active probing.
  mac.SetType ("ns3::StaWifiMac", "Ssid", SsidValue (ssid2), "ActiveProbing", BooleanValue (false));
  stas2Device = wifi.Install(phy, mac, wifiAp2StaNodes );
  mac.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssid2));
  ap2Device   = wifi.Install(phy, mac, wifiAp2Node);

  //------- Network AP3-------
  NetDeviceContainer stas3Device, ap3Device;    // station devices in AP3 network, and the AP3 itself
  Ssid ssid3 = Ssid ("ssid-AP3");
  // We want to make sure that our stations don't perform active probing.
  mac.SetType ("ns3::StaWifiMac", "Ssid", SsidValue (ssid3), "ActiveProbing", BooleanValue (false));
  stas3Device = wifi.Install(phy, mac, wifiAp3StaNodes );
  mac.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssid3));
  ap3Device   = wifi.Install(phy, mac, wifiAp3Node);

  /* We have configured Wifi for all of our STA nodes, and now we need to configure the AP (access point) node.
     We begin this process by changing the default `Attributes` of the `NqosWifiMacHelper` to reflect the requirements of the AP.
  */

  /* Now we are going to add `mobility models`. We want the STA nodes to be mobile, wandering around inside a bounding box,
     and we want to make the AP node stationary. We use the `MobilityHelper` to make this easy for us.
  */
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
    "MinX",      DoubleValue (0),
    "MinY",      DoubleValue (25),
    "DeltaX",    DoubleValue (5),
    "DeltaY",    DoubleValue (10),
    "GridWidth", UintegerValue(3),
    "LayoutType",StringValue ("RowFirst")
    );    // "GridWidth", UintegerValue(3),
  // This code tells the mobility helper to use a two-dimensional grid to initially place the STA nodes.
  // feel free to refer to `ns3::RandomWalk2dMobilityModel` which has the nodes move in a random direction
  // at ta random speed around inside a bounding box.
  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel", 
    "Bounds", RectangleValue (Rectangle (-50, 50, -50, 50)));

  mobility.Install (wifiAp1StaNodes);
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
    "MinX",      DoubleValue (20),
    "MinY",      DoubleValue (25),
    "DeltaX",    DoubleValue (5),
    "DeltaY",    DoubleValue (10),
    "GridWidth", UintegerValue(3),
    "LayoutType",StringValue ("RowFirst")
    );    // "GridWidth", UintegerValue(3),
  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel", 
    "Bounds", RectangleValue (Rectangle (-50, 50, -50, 50)));
  mobility.Install (wifiAp2StaNodes);
  

  MobilityHelper mobility2;
  // We want the AP to remain in a fixed position during the simulation
  mobility2.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  // only stations in AP1 and AP2 is mobile, the only station in AP3 is not mobile.
  mobility2.Install (wifiApsNode);
  mobility2.Install (wifiAp3StaNodes);
  mobility2.Install (switchesNode);
  mobility2.Install (terminalsNode);
  mobility2.Install (switchesNode);


  NS_LOG_INFO ("-----Building Topology------");
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", DataRateValue (100000000));   // 100M bandwidth
  csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));   // 2ms delay

  // Create the csma links, from each AP to the switch
  NetDeviceContainer terminal1Device, terminal2Device;
  NetDeviceContainer switch1Device, switch2Device;
  NetDeviceContainer link;

  //Connect ofSwitch1 to ofSwitch2  
  link = csma.Install(NodeContainer(switchesNode.Get(0),switchesNode.Get(1)));  
  switch1Device.Add(link.Get(0));
  switch2Device.Add(link.Get(1));
  
  //Connect AP1, AP2 and AP3 to ofSwitch1  
  // the AP[i] connect to Switch1,namely switchesNode.Get(0)
  link = csma.Install(NodeContainer(wifiApsNode.Get(0),switchesNode.Get(0)));
  // link is a list, including the two nodes
  // add one to apDevice{A,B,C}, the other to switch1Device
  ap1Device.Add(link.Get(0));  
  switch1Device.Add(link.Get(1));
  link = csma.Install(NodeContainer(wifiApsNode.Get(1),switchesNode.Get(0)));
  ap2Device.Add(link.Get(0));  
  switch1Device.Add(link.Get(1));
  link = csma.Install(NodeContainer(wifiApsNode.Get(2),switchesNode.Get(0)));
  ap3Device.Add(link.Get(0));
  switch1Device.Add(link.Get(1));


  //Connect terminal1 and terminal2 to ofSwitch2  
  link = csma.Install(NodeContainer(terminalsNode.Get(0),switchesNode.Get(1)));
  terminal1Device.Add(link.Get(0));
  switch2Device.Add(link.Get(1));
  link = csma.Install(NodeContainer(terminalsNode.Get(1),switchesNode.Get(1)));
  terminal2Device.Add(link.Get(0));
  switch2Device.Add(link.Get(1));


  //Create the switch netdevice,which will do the packet switching
  Ptr<Node> switchNode1 = switchesNode.Get (0);
  Ptr<Node> switchNode2 = switchesNode.Get (1);
  
  OpenFlowSwitchHelper switchHelper;

  if (use_drop)
    {
      Ptr<ns3::ofi::DropController> controller = CreateObject<ns3::ofi::DropController> ();
      switchHelper.Install (switchNode1, switch1Device, controller);
      switchHelper.Install (switchNode2, switch2Device, controller);
      //Ptr<ns3::ofi::DropController> controller2 = CreateObject<ns3::ofi::DropController> ();
      //switchHelper.Install (switchNode2, switch2Device, controller2);
    }
  else
    {
      Ptr<ns3::ofi::LearningController> controller = CreateObject<ns3::ofi::LearningController> ();
      if (!timeout.IsZero ()) controller->SetAttribute ("ExpirationTime", TimeValue (timeout));
      switchHelper.Install (switchNode1, switch1Device, controller);
      //switchHelper.Install (switchNode2, switch2Device, controller);
      Ptr<ns3::ofi::LearningController> controller2 = CreateObject<ns3::ofi::LearningController> ();
      if (!timeout.IsZero ()) controller2->SetAttribute ("ExpirationTime", TimeValue (timeout));
      switchHelper.Install (switchNode2, switch2Device, controller2);
    }

  // Add internet stack to the terminals
  InternetStackHelper internet;
  internet.Install (terminalsNode);
  internet.Install (wifiApsNode);
  internet.Install (wifiAp1StaNodes);
  internet.Install (wifiAp2StaNodes);
  internet.Install (wifiAp3StaNodes);
  //TODO

  NS_LOG_INFO ("-----Assigning IP Addresses.-----");
  Ipv4AddressHelper address;
  address.SetBase ("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ApInterfaceA;    
  ApInterfaceA = address.Assign (ap1Device);    // Ap1: 192.168.1.1
  Ipv4InterfaceContainer StaInterfaceA;
  StaInterfaceA = address.Assign (stas1Device);
  
  
  address.SetBase ("192.168.2.0", "255.255.255.0");
  Ipv4InterfaceContainer ApInterfaceB;
  ApInterfaceB = address.Assign (ap2Device);    // Ap2: 192.168.2.1
  Ipv4InterfaceContainer StaInterfaceB;
  StaInterfaceB = address.Assign (stas2Device);
  
  
  address.SetBase ("192.168.3.0", "255.255.255.0");
  Ipv4InterfaceContainer ApInterfaceC;
  ApInterfaceC = address.Assign (ap3Device);    // Ap3: 192.168.3.1
  Ipv4InterfaceContainer StaInterfaceC;
  StaInterfaceC = address.Assign (stas3Device);
  

  // for H1
  address.SetBase ("192.168.4.0", "255.255.255.0");
  Ipv4InterfaceContainer h1Interface;
  h1Interface = address.Assign (terminal1Device);
  // for H2
  address.SetBase ("192.168.5.0", "255.255.255.0");
  Ipv4InterfaceContainer h2Interface;
  h2Interface = address.Assign (terminal2Device);


  /*
  * Empty pcap files usually come up because no packets ever leave any of the nodes and 
  * onto the channel(s). Try moving the "Ipv4GlobalRoutingHelper::PopulateRoutingTables();" line 
  * before the part where you create the application source/sink. 
  * Verify if your traffic generating nodes have routes to their destinations.
  */
  //Ipv4GlobalRoutingHelper::PopulateRoutingTables ();   //不注释这句话不能生成各个追踪文件

  // Add applications
  NS_LOG_INFO ("-----Creating Applications.-----");
  uint16_t port = 9;   // Discard port (RFC 863)
  UdpEchoServerHelper echoServer (port);  // for the server side, only one param(port) is specified
  ApplicationContainer serverApps = echoServer.Install (wifiAp1StaNodes.Get(nAp1Station-1));
  //ApplicationContainer serverApps = echoServer.Install (terminalsNode.Get(1));
  serverApps.Start (Seconds(1.0));  
  serverApps.Stop (Seconds(10.0));  
  
  UdpEchoClientHelper echoClient (StaInterfaceA.GetAddress(nAp1Station-1),port); 
  //UdpEchoClientHelper echoClient (h2Interface.GetAddress(0) ,port);    // 192.168.5.1
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1));  
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));  
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));  
  ApplicationContainer clientApps = echoClient.Install(terminalsNode.Get(0));      // H1 is the client
  clientApps.Start (Seconds(2.0));  
  clientApps.Stop (Seconds(10.0));
  
  //Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
  Simulator::Stop (Seconds (10.0));

  NS_LOG_INFO ("-----Configuring Tracing.-----");

  //
  // Configure tracing of all enqueue, dequeue, and NetDevice receive events.
  // Trace output will be sent to the file as below
  //
  if (tracing)
    {
      AsciiTraceHelper ascii;
      csma.EnablePcapAll("goal-topo");
      csma.EnableAsciiAll (ascii.CreateFileStream ("goal-topo.tr"));
      phy.EnablePcap ("goal-topo-ap1", ap1Device);
      phy.EnablePcap ("goal-topo-ap2", ap2Device);
      phy.EnablePcap ("goal-topo-ap3", ap3Device);
    }

  //
  // Also configure some tcpdump traces; each interface will be traced.
  // The output files will be named:
  //     openflow-switch-<nodeId>-<interfaceId>.pcap
  // and can be read by the "tcpdump -r" command (use "-tt" option to
  // display timestamps correctly)
  // eg. tcpdump -nn -tt -r xxx.pcap
  //
  //csma.EnablePcapAll ("goal-topo", false);

  AnimationInterface anim ("goal-topo.xml");
  anim.SetConstantPosition(switchNode1,15,10);             // s1-----node 2
  anim.SetConstantPosition(switchNode2,45,10);             // s2-----node 3
  anim.SetConstantPosition(wifiApsNode.Get(0),5,20);      // Ap1----node 4
  anim.SetConstantPosition(wifiApsNode.Get(1),25,20);      // Ap2----node 5
  anim.SetConstantPosition(wifiApsNode.Get(2),40,20);      // Ap3----node 6
  anim.SetConstantPosition(terminalsNode.Get(0),40,5);    // H1-----node 0
  anim.SetConstantPosition(terminalsNode.Get(1),45,5);    // H2-----node 1
  anim.SetConstantPosition(wifiAp3StaNodes.Get(0),35,35);  //   -----node 14

  //
  // Now, do the actual simulation.
  //
  NS_LOG_INFO ("-----Running Simulation.-----");
  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_INFO ("-----Done.-----");
  #else
  NS_LOG_INFO ("-----NS-3 OpenFlow is not enabled. Cannot run simulation.-----");
  #endif // NS3_OPENFLOW
}
