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


#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/applications-module.h"
//#include "ns3/flow-monitor-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/openflow-module.h"
#include "ns3/log.h"
#include "ns3/olsr-helper.h"

// 搞不明白 `gnuplot`和`Gnuplot2Ddatabase`要通过这两个头文件包含进来
#include "ns3/stats-module.h"
#include "ns3/random-variable-stream.h"

#include "ns3/netanim-module.h"

#include <iostream>
#include <fstream>



using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("GoalTopoScript");


bool tracing  = true;
ns3::Time timeout = ns3::Seconds (0);

/*这里20.0, 30.0, 40.0都是一样的*/
ns3::Time stopTime = ns3::Seconds (20.0);  // when the simulation stops

uint32_t nAp         = 3;
uint32_t nSwitch     = 2;
uint32_t nTerminal   = 2;
uint32_t nAp1Station = 3;
uint32_t nAp2Station = 4;
uint32_t nAp3Station = 1;


//std::string str_outputFileName = "goal-topo.plt" ;
std::ofstream outputFileName("goal-topo.plt");  //GenerateOutput()接收的是ofstream类型的 
ns3::Time nSamplingPeriod = ns3::Seconds (0.5);   // 抽样间隔，根据总的Simulation时间做相应的调整


/*这里不管是2000还是4000还是5000，吞吐量都是40878----*/
// for udp-server-client application.
uint32_t nMaxPackets = 2000;    // The maximum packets to be sent.
ns3::Time nInterval  = ns3::Seconds (0.01);  // The interval between two packet sent.

// for tcp-bulk-send application.    Zero is unlimited.
uint32_t nMaxBytes = 0;



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


bool
CommandSetup (int argc, char **argv)
{
  // for commandline input
  CommandLine cmd;
  //cmd.AddValue ("packetSize", "packet size", packetSize);
  //cmd.AddValue ("nAp1Station", "Number of wifi STA devices of AP1", nAp1Station);
  //cmd.AddValue ("nAp2Station", "Number of wifi STA devices of AP2", nAp2Station);
  //cmd.AddValue ("nAp3Station", "Number of wifi STA devices of AP3", nAp3Station);

  //cmd.AddValue ("t", "Learning Controller Timeout (has no effect if drop controller is specified).", MakeCallback ( &SetTimeout));
  //cmd.AddValue ("timeout", "Learning Controller Timeout (has no effect if drop controller is specified).", MakeCallback ( &SetTimeout));

  
  // for udp-server-client application
  //cmd.AddValue ("MaxPackets", "The total packets that are available to be scheduled by the UDP application.", nMaxPackets);
  //cmd.AddValue ("Interval", "The interval between two packet sent", nInterval);

  // tcp-bulk-send application. 
  cmd.AddValue ("MaxBytes", "The amount of data to send in bytes", nMaxBytes);
  cmd.AddValue ("SamplingPeriod", "sampling period", nSamplingPeriod);
  
  cmd.Parse (argc, argv);
  return true;
}



void
CheckThroughput (FlowMonitorHelper* fmhelper, Ptr<FlowMonitor> flowMon, Gnuplot2dDataset* dataset)
{
  
  flowMon->CheckForLostPackets ();
  std::map<FlowId, FlowMonitor::FlowStats> stats = flowMon->GetFlowStats ();
  /* since fmhelper is a pointer, we should use it as a pointer.
   * `fmhelper->GetClassifier ()` instead of `fmhelper.GetClassifier ()`
   */
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (fmhelper->GetClassifier ());
  double localThrou = 0;
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin (); i != stats.end (); ++i)
    {
    /* 
     * `Ipv4FlowClassifier`
     * Classifies packets by looking at their IP and TCP/UDP headers. 
     * FiveTuple五元组是：(source-ip, destination-ip, protocol, source-port, destination-port)
    */

    // 每个flow是根据包的五元组(协议，源IP/端口，目的IP/端口)来区分的
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
    if ((t.sourceAddress=="10.0.3.2" && t.destinationAddress == "192.168.0.8")) // `10.0.3.2`是client(Node#14)的IP, `192.168.0.8`是server(Node#6)的IP
      {
          std::cout << "Flow " << i->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
          localThrou = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds())/1024/1024 ;
          std::cout << "  Throughput: " <<  localThrou << " Mbps\n";
      }
     }
  

  dataset->SetTitle ("Throughput VS Time");
  dataset->SetStyle (Gnuplot2dDataset::LINES);
  dataset->Add ((Simulator::Now ()).GetSeconds (), localThrou);

  //check throughput every nSamplingPeriod second(每隔nSamplingPeriod调用依次Simulation)
  // 表示每隔nSamplingPeriod时间，即0.5秒
  Simulator::Schedule (nSamplingPeriod, &CheckThroughput, fmhelper, flowMon, dataset);
  //这里的这个Simulator::Schedule() 与后面main()里面传的参数格式不一样。
}


int
main (int argc, char *argv[])
{

  #ifdef NS3_OPENFLOW
  Config::SetDefault ("ns3::Ipv4GlobalRouting::RespondToInterfaceEvents", BooleanValue (true));
  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold",UintegerValue (10));
  
  // 设置命令行参数
  CommandSetup (argc, argv) ;
  

  //------- for gnuplot ------
  Gnuplot gnuplot;
  Gnuplot2dDataset dataset;

  //----- init Helpers -----
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", DataRateValue (100000000));   // 100M bandwidth
  csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));   // 2ms delay
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  wifiPhy.SetChannel (wifiChannel.Create());
  //wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);
  WifiHelper wifi;
  // The SetRemoteStationManager method tells the helper the type of `rate control algorithm` to use. 
  // Here, it is asking the helper to use the AARF algorithm
  wifi.SetRemoteStationManager ("ns3::AarfWifiManager");
  //wifi.SetStandard (WIFI_PHY_STANDARD_80211n_5GHZ);
  WifiMacHelper wifiMac;

 
  NS_LOG_INFO ("-----Creating nodes-----");
 

  NodeContainer switchesNode;
  switchesNode.Create (nSwitch);    //2 Nodes(switch1 and switch2)-----node 0,1
  
  NodeContainer apsNode;
  apsNode.Create (nAp);             //3 Nodes(Ap1 Ap2 and Ap3)-----node 2,3,4
  NodeContainer wifiAp1Node = apsNode.Get (0);
  NodeContainer wifiAp2Node = apsNode.Get (1);
  NodeContainer wifiAp3Node = apsNode.Get (2);

  NodeContainer terminalsNode;
  terminalsNode.Create (nTerminal); //2 Nodes(terminal1 and terminal2)-----node 5,6
  
  NodeContainer csmaNodes;
  csmaNodes.Add(apsNode);         // APs index : 0,1,2
  csmaNodes.Add(terminalsNode);   // terminals index: 3,4 

  

  NetDeviceContainer csmaDevices;
  csmaDevices = csma.Install (csmaNodes);

  // Creating every  Ap's stations
  NodeContainer wifiAp1StaNodes;
  wifiAp1StaNodes.Create(nAp1Station);    // node 7,8,9

  NodeContainer wifiAp2StaNodes;
  wifiAp2StaNodes.Create(nAp2Station);    // node 10,11,12,13

  NodeContainer wifiAp3StaNodes;
  wifiAp3StaNodes.Create(nAp3Station);    //  node 14

  

  NS_LOG_INFO ("-----Building Topology------");

  // Create the csma links, from each AP & terminals to the switch
  NetDeviceContainer csmaAp1Device, csmaAp2Device, csmaAp3Device;
  csmaAp1Device.Add (csmaDevices.Get(0));
  csmaAp2Device.Add (csmaDevices.Get(1));
  csmaAp3Device.Add (csmaDevices.Get(2));

  NetDeviceContainer terminalsDevice;
  terminalsDevice.Add (csmaDevices.Get(3));
  terminalsDevice.Add (csmaDevices.Get(4));
  
  NetDeviceContainer switch1Device, switch2Device;
  NetDeviceContainer link;

  //Connect ofSwitch1 to ofSwitch2  
  link = csma.Install(NodeContainer(switchesNode.Get(0),switchesNode.Get(1)));  
  switch1Device.Add(link.Get(0));
  switch2Device.Add(link.Get(1));
  
  //Connect AP1, AP2 and AP3 to ofSwitch1  
  link = csma.Install(NodeContainer(csmaNodes.Get(0),switchesNode.Get(0)));
  // link is a list, including the two nodes
  // add one to apDevice{A,B,C}, the other to switch1Device
  csmaAp1Device.Add(link.Get(0));  
  switch1Device.Add(link.Get(1));
  link = csma.Install(NodeContainer(csmaNodes.Get(1),switchesNode.Get(0)));
  csmaAp2Device.Add(link.Get(0));  
  switch1Device.Add(link.Get(1));
  link = csma.Install(NodeContainer(csmaNodes.Get(2),switchesNode.Get(0)));
  csmaAp3Device.Add(link.Get(0));
  switch1Device.Add(link.Get(1));


  //Connect terminal1 and terminal2 to ofSwitch2  
  for (int i = 3; i < 5; i++)
    {
      link = csma.Install(NodeContainer(csmaNodes.Get(i), switchesNode.Get(1)));
      terminalsDevice.Add(link.Get(0));
      switch2Device.Add(link.Get(1));

    }


  //------- Network AP1-------
  NetDeviceContainer wifiSta1Device, wifiAp1Device;
  Ssid ssid1 = Ssid ("ssid-AP1");
  // We want to make sure that our stations don't perform active probing.(就是等AP发现STA，而STA不主动发现AP)
  wifiMac.SetType ("ns3::StaWifiMac", "Ssid", SsidValue (ssid1), "ActiveProbing", BooleanValue (false));
  wifiSta1Device = wifi.Install(wifiPhy, wifiMac, wifiAp1StaNodes );
  wifiMac.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssid1));
  wifiAp1Device   = wifi.Install(wifiPhy, wifiMac, wifiAp1Node);    // csmaNodes

  //------- Network AP2-------
  NetDeviceContainer wifiSta2Device, wifiAp2Device;
  Ssid ssid2 = Ssid ("ssid-AP2");
  // We want to make sure that our stations don't perform active probing.
  wifiMac.SetType ("ns3::StaWifiMac", "Ssid", SsidValue (ssid2), "ActiveProbing", BooleanValue (false));
  wifiSta2Device = wifi.Install(wifiPhy, wifiMac, wifiAp2StaNodes );
  wifiMac.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssid2));
  wifiAp2Device   = wifi.Install(wifiPhy, wifiMac, wifiAp2Node);     // csmaNodes

  //------- Network AP3-------
  NetDeviceContainer wifiSta3Device, wifiAp3Device;
  Ssid ssid3 = Ssid ("ssid-AP3");
  wifiMac.SetType ("ns3::StaWifiMac", "Ssid", SsidValue (ssid3), "ActiveProbing", BooleanValue (false));
  wifiSta3Device = wifi.Install(wifiPhy, wifiMac, wifiAp3StaNodes );
  wifiMac.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssid3));
  wifiAp3Device   = wifi.Install(wifiPhy, wifiMac, wifiAp3Node);    // csmaNodes

  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
    "MinX",      DoubleValue (0),
    "MinY",      DoubleValue (25),
    "DeltaX",    DoubleValue (5),
    "DeltaY",    DoubleValue (5),
    "GridWidth", UintegerValue(3),
    "LayoutType",StringValue ("RowFirst")
    );    // "GridWidth", UintegerValue(3),
  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel", 
    "Bounds", RectangleValue (Rectangle (-50, 50, -50, 50)));
  mobility.Install (wifiAp1StaNodes);

  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
    "MinX",      DoubleValue (25),
    "MinY",      DoubleValue (25),
    "DeltaX",    DoubleValue (5),
    "DeltaY",    DoubleValue (5),
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
  mobility2.Install (csmaNodes);    // csmaNodes includes APs and terminals
  mobility2.Install (wifiAp3StaNodes);
  mobility2.Install (switchesNode);

  //Create the switch netdevice,which will do the packet switching
  Ptr<Node> switchNode1 = switchesNode.Get (0);
  Ptr<Node> switchNode2 = switchesNode.Get (1);
  
  OpenFlowSwitchHelper switchHelper;


  Ptr<ns3::ofi::LearningController> controller = CreateObject<ns3::ofi::LearningController> ();
  if (!timeout.IsZero ()) controller->SetAttribute ("ExpirationTime", TimeValue (timeout));
  switchHelper.Install (switchNode1, switch1Device, controller);
  //switchHelper.Install (switchNode2, switch2Device, controller);
  Ptr<ns3::ofi::LearningController> controller2 = CreateObject<ns3::ofi::LearningController> ();
  if (!timeout.IsZero ()) controller2->SetAttribute ("ExpirationTime", TimeValue (timeout));
  switchHelper.Install (switchNode2, switch2Device, controller2);


  // We enable OLSR (which will be consulted at a higher priority than
  // the global routing) on the backbone nodes
  NS_LOG_INFO ("Enabling OLSR routing");
  OlsrHelper olsr;
  
  Ipv4StaticRoutingHelper ipv4RoutingHelper;

  Ipv4ListRoutingHelper list;
  list.Add (ipv4RoutingHelper, 0);
  list.Add (olsr, 10);

  // Add internet stack to the terminals
  InternetStackHelper internet;
  internet.SetRoutingHelper (list); // has effect on the next Install ()
  internet.Install (csmaNodes);
  internet.Install (wifiAp1StaNodes);
  internet.Install (wifiAp2StaNodes);
  internet.Install (wifiAp3StaNodes);

  NS_LOG_INFO ("-----Assigning IP Addresses.-----");

  Ipv4AddressHelper csmaIpAddress;
  csmaIpAddress.SetBase ("192.168.0.0", "255.255.255.0");

  // for Ap1,Ap2 and Ap3
  csmaIpAddress.Assign (csmaAp1Device);    // csmaDevices
  csmaIpAddress.Assign (csmaAp2Device); 
  //csmaIpAddress.Assign (csmaAp3Device);
  Ipv4InterfaceContainer csmaAp3Interface;
  csmaAp3Interface = csmaIpAddress.Assign (csmaAp3Device);
  Ipv4InterfaceContainer h1h2Interface;
  h1h2Interface = csmaIpAddress.Assign (terminalsDevice); 


  Ipv4AddressHelper ap1IpAddress;
  ap1IpAddress.SetBase ("10.0.1.0", "255.255.255.0");
  NetDeviceContainer wifi1Device = wifiSta1Device;
  wifi1Device.Add(wifiAp1Device);
  Ipv4InterfaceContainer interfaceA ;
  interfaceA = ap1IpAddress.Assign (wifi1Device);
  

  Ipv4AddressHelper ap2IpAddress;
  ap2IpAddress.SetBase ("10.0.2.0", "255.255.255.0");
  NetDeviceContainer wifi2Device = wifiSta2Device;
  wifi2Device.Add(wifiAp2Device);
  Ipv4InterfaceContainer interfaceB ;
  interfaceB = ap2IpAddress.Assign (wifi2Device);


  Ipv4AddressHelper ap3IpAddress;
  ap3IpAddress.SetBase ("10.0.3.0", "255.255.255.0");
  //NetDeviceContainer wifi3Device = wifiSta3Device;
  //wifi3Device.Add(wifiAp3Device);
  //Ipv4InterfaceContainer interfaceC ;
  //interfaceC = ap3IpAddress.Assign (wifi3Device);
  Ipv4InterfaceContainer apWifiInterfaceC ;
  Ipv4InterfaceContainer staWifiInterfaceC ;
  apWifiInterfaceC  = ap3IpAddress.Assign (wifiAp3Device);
  staWifiInterfaceC = ap3IpAddress.Assign (wifiSta3Device);



  // -----for StaticRouting(its very useful)-----
  
  Ptr<Ipv4> ipv4Ap3 = apsNode.Get(2)->GetObject<Ipv4> ();
  Ptr<Ipv4> ipv4H2 = terminalsNode.Get(1)->GetObject<Ipv4> ();    // or csmaNodes.Get(4)
  Ptr<Ipv4> ipv4Ap3Sta = wifiAp3StaNodes.Get(0)->GetObject<Ipv4> ();    // node 14

  //Ipv4StaticRoutingHelper ipv4RoutingHelper;   // moved this code ahead
  // the intermedia AP3
  //Ptr<Ipv4StaticRouting> staticRoutingAp3 = ipv4RoutingHelper.GetStaticRouting (ipv4Ap3);
  //staticRoutingAp3->SetDefaultRoute(h1h2Interface.GetAddress(1), 1);
  //staticRoutingAp3->SetDefaultRoute(staWifiInterfaceC.GetAddress(0), 1);
  // the server
  Ptr<Ipv4StaticRouting> staticRoutingH2 = ipv4RoutingHelper.GetStaticRouting (ipv4H2);
  staticRoutingH2->SetDefaultRoute(csmaAp3Interface.GetAddress(0), 1);
  // the client
  Ptr<Ipv4StaticRouting> staticRoutingAp3Sta = ipv4RoutingHelper.GetStaticRouting (ipv4Ap3Sta);
  staticRoutingAp3Sta->SetDefaultRoute(apWifiInterfaceC.GetAddress(0), 1);
  

  // Add applications
  NS_LOG_INFO ("-----Creating Applications.-----");
  uint16_t port = 9;   // Discard port (RFC 863)
  


  /*
  UdpServerHelper server (port);  // for the server side, only one param(port) is specified
  ApplicationContainer serverApps = server.Install (terminalsNode.Get(1));
  serverApps.Start (Seconds(1.0));  
  serverApps.Stop (stopTime);  
  

  UdpClientHelper client (h1h2Interface.GetAddress(1) ,port);
  client.SetAttribute ("MaxPackets", UintegerValue (nMaxPackets));
  // if only 1, the switch could not learn, 5 is too much, which we don't need. 2 is proper
  client.SetAttribute ("Interval", TimeValue (nInterval));  
  client.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = client.Install(wifiAp3StaNodes.Get(0));    //terminalsNode.Get(0), wifiAp3Node
  clientApps.Start (Seconds(2.0));  
  clientApps.Stop (stopTime);
  */



  // server
  PacketSinkHelper sink ("ns3::TcpSocketFactory",
                         InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApps = sink.Install (terminalsNode.Get(1));
  sinkApps.Start (Seconds (1.0));
  sinkApps.Stop (stopTime);


  // client
  BulkSendHelper source ("ns3::TcpSocketFactory",
                         InetSocketAddress (h1h2Interface.GetAddress(1), port));
  // Set the amount of data to send in bytes.  Zero is unlimited.
  source.SetAttribute ("MaxBytes", UintegerValue (nMaxBytes));
  ApplicationContainer sourceApps = source.Install (wifiAp3StaNodes.Get(0));
  sourceApps.Start (Seconds (2.0));
  sourceApps.Stop (Seconds (10.0));

  
  // GlobalRouting does NOT work with Wi-Fi.
  // https://groups.google.com/forum/#!searchin/ns-3-users/wifi$20global$20routing/ns-3-users/Z9K1YrEmbcI/MrP2k47HAQAJ
  //Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  NS_LOG_INFO ("-----Configuring Tracing.-----");

  //
  // Configure tracing of all enqueue, dequeue, and NetDevice receive events.
  // Trace output will be sent to the file as below
  //
  if (tracing)
    {
      AsciiTraceHelper ascii;
      //csma.EnablePcapAll("goal-topo");
      csma.EnableAsciiAll (ascii.CreateFileStream ("trace/goal-topo.tr"));
      wifiPhy.EnablePcap ("trace/goal-topo-ap1-wifi", wifiAp1Device);
      wifiPhy.EnablePcap ("trace/goal-topo-ap2-wifi", wifiAp2Device);
      wifiPhy.EnablePcap ("trace/goal-topo-ap3-wifi", wifiAp3Device);
      wifiPhy.EnablePcap ("trace/goal-topo-ap3-sta1-wifi", wifiSta3Device);
      // WifiMacHelper doesnot have `EnablePcap()` method
      csma.EnablePcap ("trace/goal-topo-switch1-csma", switch1Device);
      csma.EnablePcap ("trace/goal-topo-switch1-csma", switch2Device);
      csma.EnablePcap ("trace/goal-topo-ap1-csma", csmaAp1Device);
      csma.EnablePcap ("trace/goal-topo-ap2-csma", csmaAp2Device);
      csma.EnablePcap ("trace/goal-topo-ap3-csma", csmaAp3Device);
      csma.EnablePcap ("trace/goal-topo-H1-csma", terminalsDevice.Get(0));
      csma.EnablePcap ("trace/goal-topo-H2-csma", terminalsDevice.Get(1));
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

  AnimationInterface anim ("trace/goal-topo.xml");
  anim.SetConstantPosition(switchNode1,30,10);             // s1-----node 0
  anim.SetConstantPosition(switchNode2,65,10);             // s2-----node 1
  anim.SetConstantPosition(apsNode.Get(0),5,20);      // Ap1----node 2
  anim.SetConstantPosition(apsNode.Get(1),30,20);      // Ap2----node 3
  anim.SetConstantPosition(apsNode.Get(2),55,20);      // Ap3----node 4
  anim.SetConstantPosition(terminalsNode.Get(0),60,25);    // H1-----node 5
  anim.SetConstantPosition(terminalsNode.Get(1),65,25);    // H2-----node 6
  anim.SetConstantPosition(wifiAp3StaNodes.Get(0),55,30);  //   -----node 14

  anim.EnablePacketMetadata();   // to see the details of each packet






/*
** Calculate Throughput using Flowmonitor
** 每个探针(probe)会根据四点来对包进行分类
** -- when packet is `sent`;
** -- when packet is `forwarded`;
** -- when packet is `received`;
** -- when packet is `dropped`;
** 由于包是在IP层进行track的，所以任何的四层(TCP)重传的包，都会被认为是一个新的包
*/
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();
  //表示从第#1秒开始
  Simulator::Schedule(Seconds(1),&CheckThroughput, &flowmon, monitor, &dataset);

  NS_LOG_INFO ("-----Running Simulation.-----");
  /* 以下的 Simulation::Stop() 和 Simulator::Run () 的顺序
   * 是根据 `ns3-lab-loaded-from-internet/lab1-task1-appelman.cc` 来的
   */
  Simulator::Stop (stopTime);
  Simulator::Run ();

  // 测吞吐量
  CheckThroughput(&flowmon, monitor, &dataset);


  // monitor->SerializeToXmlFile("trace/goal-topo.flowmon", true, true);
  /* the SerializeToXmlFile () function 2nd and 3rd parameters 
   * are used respectively to activate/deactivate the histograms and the per-probe detailed stats.
   */





  Simulator::Destroy ();
  NS_LOG_INFO ("-----Done.-----");
  gnuplot.AddDataset (dataset);
  gnuplot.GenerateOutput (outputFileName);
  NS_LOG_INFO ("-----Added dataset to outputfile-----");
  #else
  NS_LOG_INFO ("-----NS-3 OpenFlow is not enabled. Cannot run simulation.-----");
  #endif // NS3_OPENFLOW
}
