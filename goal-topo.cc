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
//#include "ns3/constant-velocity-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/applications-module.h"
//#include "ns3/flow-monitor-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/openflow-module.h"
#include "ns3/log.h"
#include "ns3/olsr-helper.h"

//包含 `gnuplot`和`Gnuplot2Ddatabase`
#include "ns3/stats-module.h"
#include "ns3/random-variable-stream.h"

#include "ns3/netanim-module.h"

#include <iostream>
#include <fstream>
#include <vector>



using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("GoalTopoScript");


bool tracing  = true;
ns3::Time timeout = ns3::Seconds (0);


double stopTime = 50.0;  // when the simulation stops

uint32_t nAp         = 3;
uint32_t nSwitch     = 2;
uint32_t nHost       = 2;
uint32_t nAp1Station = 3;
uint32_t nAp2Station = 4;
uint32_t nAp3Station = 1;


double nSamplingPeriod = 0.1;   // 抽样间隔，根据总的Simulation时间做相应的调整


/* for udp-server-client application. */
uint32_t nMaxPackets = 20000;    // The maximum packets to be sent.
double nInterval  = 0.01;  // The interval between two packet sent.
uint32_t nPacketSize = 1024;

/* for tcp-bulk-send application. */   
uint32_t nMaxBytes = 0;  //Zero is unlimited.



/* 恒定速度移动节点的
初始位置 x = 0.0, y = 25.0
和
移动速度 x = 1.0,  y=  0.0
*/
Vector3D mPosition = Vector3D(0.0, 25.0, 0.0);
Vector3D mVelocity = Vector3D(1.0, 0.0 , 0.0);


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

  CommandLine cmd;
  //cmd.AddValue ("t", "Learning Controller Timeout (has no effect if drop controller is specified).", MakeCallback ( &SetTimeout));
  //cmd.AddValue ("timeout", "Learning Controller Timeout (has no effect if drop controller is specified).", MakeCallback ( &SetTimeout));

  cmd.AddValue ("SamplingPeriod", "Sampling period", nSamplingPeriod);
  cmd.AddValue ("stopTime", "The time to stop", stopTime);
  
  /* for udp-server-client application */
  cmd.AddValue ("MaxPackets", "The total packets available to be scheduled by the UDP application.", nMaxPackets);
  cmd.AddValue ("Interval", "The interval between two packet sent", nInterval);
  cmd.AddValue ("PacketSize", "The size in byte of each packet", nPacketSize);

  /* for tcp-bulk-send application. */
  
  //cmd.AddValue ("MaxBytes", "The amount of data to send in bytes", nMaxBytes);
  
  cmd.Parse (argc, argv);
  return true;
}



/*
 * Calculate Throughput using Flowmonitor
 * 每个探针(probe)会根据四点来对包进行分类
 * -- when packet is `sent`;
 * -- when packet is `forwarded`;
 * -- when packet is `received`;
 * -- when packet is `dropped`;
 * 由于包是在IP层进行track的，所以任何的四层(TCP)重传的包，都会被认为是一个新的包
 */
void
ThroughputMonitor (FlowMonitorHelper* fmhelper, Ptr<FlowMonitor> monitor, 
  Gnuplot2dDataset dataset)
{
  
  double throu   = 0.0;
  monitor->CheckForLostPackets ();
  std::map<FlowId, FlowMonitor::FlowStats> flowStats = monitor->GetFlowStats ();
  /* since fmhelper is a pointer, we should use it as a pointer.
   * `fmhelper->GetClassifier ()` instead of `fmhelper.GetClassifier ()`
   */
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (fmhelper->GetClassifier ());
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = flowStats.begin (); i != flowStats.end (); ++i)
    {
    /* 
     * `Ipv4FlowClassifier`
     * Classifies packets by looking at their IP and TCP/UDP headers. 
     * FiveTuple五元组是：(source-ip, destination-ip, protocol, source-port, destination-port)
    */

    /* 每个flow是根据包的五元组(协议，源IP/端口，目的IP/端口)来区分的 */
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
    // `192.168.0.11`是client(Node #14)的IP,
    // `192.168.0.7` 是client(Node #10)的IP
    if ((t.sourceAddress=="192.168.0.11" && t.destinationAddress == "10.0.0.5"))
      {
          // UDP_PROT_NUMBER = 17
          if (17 == unsigned(t.protocol))
          {
            throu   = i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds())/1024 ;
            dataset.Add  (Simulator::Now().GetSeconds(), throu);
          }
          else
          {
            std::cout << "This is not UDP traffic" << std::endl;
          }
      }

    }
  /* check throughput every nSamplingPeriod second(每隔nSamplingPeriod调用依次Simulation)
   * 表示每隔nSamplingPeriod时间
   */
  Simulator::Schedule (Seconds(nSamplingPeriod), &ThroughputMonitor, fmhelper, monitor, 
    dataset);

}


void
DelayMonitor (FlowMonitorHelper* fmhelper, Ptr<FlowMonitor> monitor, 
  Gnuplot2dDataset dataset1)
{
  
  double delay   = 0.0;
  monitor->CheckForLostPackets ();
  std::map<FlowId, FlowMonitor::FlowStats> flowStats = monitor->GetFlowStats ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (fmhelper->GetClassifier ());
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = flowStats.begin (); i != flowStats.end (); ++i)
    {

    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
    if ((t.sourceAddress=="192.168.0.11" && t.destinationAddress == "10.0.0.5"))
      {
          // UDP_PROT_NUMBER = 17
          if (17 == unsigned(t.protocol))
          {
            delay   = i->second.delaySum.GetSeconds ();
            dataset1.Add (Simulator::Now().GetSeconds(), delay);
          }
          else
          {
            std::cout << "This is not UDP traffic" << std::endl;
          }
      }

    }
  Simulator::Schedule (Seconds(nSamplingPeriod), &DelayMonitor, fmhelper, monitor, 
    dataset1);

}


void
LostPacketsMonitor (FlowMonitorHelper* fmhelper, Ptr<FlowMonitor> monitor, 
  Gnuplot2dDataset dataset2)
{
  
  double packets = 0.0;
  monitor->CheckForLostPackets ();
  std::map<FlowId, FlowMonitor::FlowStats> flowStats = monitor->GetFlowStats ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (fmhelper->GetClassifier ());
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = flowStats.begin (); i != flowStats.end (); ++i)
    {

    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
    if ((t.sourceAddress=="192.168.0.11" && t.destinationAddress == "10.0.0.5"))
      {
          // UDP_PROT_NUMBER = 17
          if (17 == unsigned(t.protocol))
          {
            packets = i->second.lostPackets;
            dataset2.Add (Simulator::Now().GetSeconds(), packets);
          }
          else
          {
            std::cout << "This is not UDP traffic" << std::endl;
          }
      }

    }
  Simulator::Schedule (Seconds(nSamplingPeriod), &LostPacketsMonitor, fmhelper, monitor, 
    dataset2);

}


void
JitterMonitor (FlowMonitorHelper* fmhelper, Ptr<FlowMonitor> monitor, 
  Gnuplot2dDataset dataset3)
{
  
  double jitter  = 0.0;
  monitor->CheckForLostPackets ();
  std::map<FlowId, FlowMonitor::FlowStats> flowStats = monitor->GetFlowStats ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (fmhelper->GetClassifier ());
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = flowStats.begin (); i != flowStats.end (); ++i)
    {

    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
    if ((t.sourceAddress=="192.168.0.11" && t.destinationAddress == "10.0.0.5"))
      {
          // UDP_PROT_NUMBER = 17
          if (17 == unsigned(t.protocol))
          {
            jitter  = i->second.jitterSum.GetSeconds ();
            dataset3.Add (Simulator::Now().GetSeconds(), jitter);
          }
          else
          {
            std::cout << "This is not UDP traffic" << std::endl;
          }
      }

    }
  Simulator::Schedule (Seconds(nSamplingPeriod), &JitterMonitor, fmhelper, monitor, 
    dataset3);

}


void PrintParams (FlowMonitorHelper* fmhelper, Ptr<FlowMonitor> monitor){
  double tempThroughput = 0.0;
  monitor->CheckForLostPackets(); 
  std::map<FlowId, FlowMonitor::FlowStats> flowStats = monitor->GetFlowStats();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (fmhelper->GetClassifier());

  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = flowStats.begin (); i != flowStats.end (); i++){ 
      // A tuple: Source-ip, destination-ip, protocol, source-port, destination-port
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
      

    if ((t.sourceAddress=="192.168.0.11" && t.destinationAddress == "10.0.0.5"))
      {
          // UDP_PROT_NUMBER = 17
          if (17 == unsigned(t.protocol))
          {
            std::cout<<"Time: " << Simulator::Now ().GetSeconds () << " s," << " Flow " << i->first  << "  Protocol  " << "UDP" << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")" << std::endl;

            std::cout<<"Tx Packets = " << i->second.txPackets<<std::endl;
            std::cout<<"Rx Packets = " << i->second.rxPackets<<std::endl;
            std::cout<<"Duration: " <<i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds()<<std::endl;
            std::cout<<"Last Received Packet: "<< i->second.timeLastRxPacket.GetSeconds()<<" Seconds"<<std::endl;
            tempThroughput = (i->second.rxBytes * 8.0 / (i->second.timeLastRxPacket.GetSeconds() - i->second.timeFirstTxPacket.GetSeconds())/1024);
            std::cout<<"Throughput: "<< tempThroughput <<" Kbps"<<std::endl;
            std::cout<< "Delay: " << i->second.delaySum.GetSeconds () << std::endl;
            std::cout<< "LostPackets: " << i->second.lostPackets << std::endl;
            std::cout<< "Jitter: " << i->second.jitterSum.GetSeconds () << std::endl;
            //std::cout<<"Last Received Packet: "<< i->second.timeLastRxPacket.GetSeconds()<<" Seconds ---->" << "Throughput: " << tempThroughput << " Kbps" << std::endl;
            std::cout<<"------------------------------------------"<<std::endl;

          }
          else
          {
            std::cout << "This is not UDP traffic" << std::endl;
          }
      }
    }
  // 每隔一秒打印一次
  Simulator::Schedule(Seconds(1), &PrintParams, fmhelper, monitor);
}



int
main (int argc, char *argv[])
{

  Config::SetDefault ("ns3::Ipv4GlobalRouting::RespondToInterfaceEvents", BooleanValue (true));
  /* RTS/CTS 一种半双工的握手协议 */
  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold",UintegerValue (10));
  /* 设置最大WIFI覆盖距离为5m, 超出这个距离之后将无法传输WIFI信号 */
  //Config::SetDefault ("ns3::RangePropagationLossModel::MaxRange", DoubleValue (5));
  
  /* 设置命令行参数 */
  CommandSetup (argc, argv) ;
  


  /*----- init Helpers ----- */
  CsmaHelper csma, csma2, csmaSwitch;
  //csma.SetChannelAttribute ("DataRate", DataRateValue (100000000));   // 100M bandwidth
  //csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));   // 2ms delay
  
  /* 调用YansWifiChannelHelper::Default() 已经添加了默认的传播损耗模型, 下面不要再手动添加 
  By default, we create a channel model with a propagation delay equal to a constant, 
  the speed of light, and a propagation loss based on a log distance model 
  with a reference loss of 46.6777 dB at reference distance of 1m. 
  */
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  ////////////////////////////////////////
  ////////////// LOSS MODEL //////////////
  ////////////////////////////////////////

  /* 
   * `FixedRssLossModel` will cause the `rss to be fixed` regardless
   * of the distance between the two stations, and the transmit power 
   *
   *
   *
   *
   *
   *
   */
  /* 传播延时速度是恒定的  */
  //wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  /* 很多地方都用这个，不知道什么意思  */
  // wifiChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel");  // !!! 加了这句之后AP和STA就无法连接了
  //wifiChannel.AddPropagationLoss ("ns3::LogDistancePropagationLossModel");
  /* 不管发送功率是多少，都返回一个恒定的接收功率  */
  //wifiChannel.AddPropagationLoss ("ns3::FixedRssLossModel","Rss",DoubleValue (rss));
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
  wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);
  wifiPhy.SetChannel (wifiChannel.Create());
  WifiHelper wifi;
  /* The SetRemoteStationManager method tells the helper the type of `rate control algorithm` to use. 
   * Here, it is asking the helper to use the AARF algorithm
   */
  wifi.SetRemoteStationManager ("ns3::AarfWifiManager");
  wifi.SetStandard (WIFI_PHY_STANDARD_80211g);
  //wifi.SetStandard (WIFI_PHY_STANDARD_80211n_5GHZ);
  //wifi.SetStandard (WIFI_PHY_STANDARD_80211b);
  WifiMacHelper wifiMac;

 
  NS_LOG_INFO ("------------Creating Nodes------------");
  NodeContainer switchesNode, apsNode, hostsNode;
  NodeContainer staWifi1Nodes, staWifi2Nodes, staWifi3Nodes;

  switchesNode. Create (nSwitch);    //2 Nodes(switch1 and switch2)-----node 0,1
  apsNode.      Create (nAp);        //3 Nodes(Ap1 Ap2 and Ap3)-----node 2,3,4
  hostsNode.    Create (nHost);      //2 Nodes(terminal1 and terminal2)-----node 5,6

  staWifi1Nodes.Create(nAp1Station);    // node 7,8,9
  staWifi2Nodes.Create(nAp2Station);    // node 10,11,12,13
  staWifi3Nodes.Create(nAp3Station);    //  node 14
  
  Ptr<Node> ap1WifiNode = apsNode.Get (0);
  Ptr<Node> ap2WifiNode = apsNode.Get (1);
  Ptr<Node> ap3WifiNode = apsNode.Get (2);
  

  /* 这里用一个`csmaNodes`来包括 `apsNode` 和 `hostsNode` */
  NodeContainer csmaNodes;
  csmaNodes.Add(apsNode);     // APs index : 0,1,2
  csmaNodes.Add(hostsNode);   // terminals index: 3,4 
  
  
  

  NS_LOG_INFO ("------------Creating Devices------------");
  /* CSMA Devices */
  NetDeviceContainer csmaDevices;
  NetDeviceContainer ap1CsmaDevice, ap2CsmaDevice, ap3CsmaDevice;
  NetDeviceContainer hostsDevice;
  NetDeviceContainer switch1Device, switch2Device;

  csmaDevices = csma.Install (csmaNodes);
  //hostsDevice.Add (csmaDevices.Get(3));
  //hostsDevice.Add (csmaDevices.Get(4));
  
  /* WIFI Devices */
  NetDeviceContainer stasWifi1Device, apWifi1Device;
  NetDeviceContainer stasWifi2Device, apWifi2Device;
  NetDeviceContainer stasWifi3Device, apWifi3Device;

  
  NS_LOG_INFO ("------------Building Topology-------------");

  NetDeviceContainer link;
  /* Create the csma links, from each AP && terminals to the switch */

  /* #1 Connect ofSwitch1 to ofSwitch2 */
  link = csmaSwitch.Install(NodeContainer(switchesNode.Get(0),switchesNode.Get(1)));  
  switch1Device.Add(link.Get(0));
  switch2Device.Add(link.Get(1));
  
  /* #2 Connect AP1, AP2 and AP3 to ofSwitch1 */  
  link = csma.Install(NodeContainer(csmaNodes.Get(0),switchesNode.Get(0)));
  ap1CsmaDevice.Add(link.Get(0));  
  switch1Device.Add(link.Get(1));
  link = csma.Install(NodeContainer(csmaNodes.Get(1),switchesNode.Get(0)));
  ap2CsmaDevice.Add(link.Get(0));  
  switch1Device.Add(link.Get(1));
  link = csma.Install(NodeContainer(csmaNodes.Get(2),switchesNode.Get(0)));
  ap3CsmaDevice.Add(link.Get(0));
  switch1Device.Add(link.Get(1));


  /* #3 Connect terminal1 and terminal2 to ofSwitch2  */
  for (int i = 3; i < 5; i++)
    {
      link = csma2.Install(NodeContainer(csmaNodes.Get(i), switchesNode.Get(1)));
      hostsDevice.Add(link.Get(0));
      switch2Device.Add(link.Get(1));

    }


  Ssid ssid1 = Ssid ("ssid-AP1");
  Ssid ssid2 = Ssid ("ssid-AP2");
  Ssid ssid3 = Ssid ("ssid-AP3");
  //----------------------- Network AP1--------------------
  /* We want to make sure that our stations don't perform active probing.
   * (就是等AP发现STA，而STA不主动发现AP)
   */
  // 从wifi-intra-handoff.cc 学的 , 发现STA和AP的wifiPhy如果不是同一个Channel, 他们是无法关联的
  //wifiPhy.Set("ChannelNumber", UintegerValue(1 + (0 % 3) * 5));   // 0

  wifiMac.SetType ("ns3::StaWifiMac", "Ssid", SsidValue (ssid1), "ActiveProbing", BooleanValue (false));
  stasWifi1Device = wifi.Install(wifiPhy, wifiMac, staWifi1Nodes );
  wifiMac.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssid1));
  apWifi1Device   = wifi.Install(wifiPhy, wifiMac, ap1WifiNode);

  //----------------------- Network AP2--------------------
  /* We want to make sure that our stations don't perform active probing. */
  // 从wifi-intra-handoff.cc 学的
  //wifiPhy.Set("ChannelNumber", UintegerValue(1 + (1 % 3) * 5));    // 1  

  wifiMac.SetType ("ns3::StaWifiMac", "Ssid", SsidValue (ssid2), "ActiveProbing", BooleanValue (false));
  stasWifi2Device = wifi.Install(wifiPhy, wifiMac, staWifi2Nodes );
  wifiMac.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssid2));
  apWifi2Device   = wifi.Install(wifiPhy, wifiMac, ap2WifiNode);

  //----------------------- Network AP3--------------------
  // 从wifi-intra-handoff.cc 学的
  //wifiPhy.Set("ChannelNumber", UintegerValue(1 + (2 % 3) * 5));    //2

  wifiMac.SetType ("ns3::StaWifiMac", "Ssid", SsidValue (ssid3), "ActiveProbing", BooleanValue (false));
  stasWifi3Device = wifi.Install(wifiPhy, wifiMac, staWifi3Nodes );
  wifiMac.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssid3));
  apWifi3Device   = wifi.Install(wifiPhy, wifiMac, ap3WifiNode);

  MobilityHelper mobility1;
  /* for staWifi--1--Nodes */
  mobility1.SetPositionAllocator ("ns3::GridPositionAllocator",
    "MinX",      DoubleValue (0),
    "MinY",      DoubleValue (30),
    "DeltaX",    DoubleValue (5),
    "DeltaY",    DoubleValue (5),
    "GridWidth", UintegerValue(3),
    "LayoutType",StringValue ("RowFirst")
    );    // "GridWidth", UintegerValue(3),
  mobility1.SetMobilityModel ("ns3::RandomWalk2dMobilityModel", 
    "Bounds", RectangleValue (Rectangle (-50, 50, -50, 50)));
  mobility1.Install (staWifi1Nodes);

  /* for staWifi--2--Nodes */
  MobilityHelper mobility2;
  mobility2.SetPositionAllocator ("ns3::GridPositionAllocator",
    "MinX",      DoubleValue (25),
    "MinY",      DoubleValue (30),
    "DeltaX",    DoubleValue (10),
    "DeltaY",    DoubleValue (10),
    "GridWidth", UintegerValue(2),
    "LayoutType",StringValue ("RowFirst")
    );    // "GridWidth", UintegerValue(3),
  mobility2.SetMobilityModel ("ns3::RandomWalk2dMobilityModel", 
    "Bounds", RectangleValue (Rectangle (-50, 50, -50, 50)));
  mobility2.Install (staWifi2Nodes);



  /* for sta-1-Wifi-3-Node 要让Wifi3网络中的Sta1以恒定速度移动  */
  MobilityHelper mobConstantSpeed;
  mobConstantSpeed.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
  mobConstantSpeed.Install (staWifi3Nodes.Get(0));  // Wifi-3中的第一个节点(即Node14)安装
  
  Ptr <ConstantVelocityMobilityModel> velocityModel = staWifi3Nodes.Get(0)->GetObject<ConstantVelocityMobilityModel>();
  velocityModel->SetPosition(mPosition);
  velocityModel->SetVelocity(mVelocity);


  /* for ConstantPosition Nodes */
  MobilityHelper mobConstantPosition;
  /* We want the AP to remain in a fixed position during the simulation 
   * only stations in AP1 and AP2 is mobile, the only station in AP3 is not mobile.
   */
  mobConstantPosition.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobConstantPosition.Install (csmaNodes);    // csmaNodes includes APs and terminals
  //mobConstantPosition.Install (staWifi3Nodes);
  mobConstantPosition.Install (switchesNode);

  /* Create the switch netdevice,which will do the packet switching */
  Ptr<Node> switchNode1 = switchesNode.Get (0);
  Ptr<Node> switchNode2 = switchesNode.Get (1);
  
  /*------------------ OpenFlow Switch && Controller------------*/
  OpenFlowSwitchHelper switchHelper;
  /* OpenFlowSwitchNetDevice::SetController (Ptr<ofi::Controller>), 
     Ptr<Node>::AddDevice (OpenFlowSwitchNetDevice), 
     OpenFlowSwitchNetDevice::AddSwitchPort()
  是在
  OpenFlowSwitchHelper::Install()方法的实现中调用的

  */

  Ptr<ns3::ofi::LearningController> controller = CreateObject<ns3::ofi::LearningController> ();
  if (!timeout.IsZero ()) controller->SetAttribute ("ExpirationTime", TimeValue (timeout));
  switchHelper.Install (switchNode1, switch1Device, controller);
  //switchHelper.Install (switchNode2, switch2Device, controller);

  Ptr<ns3::ofi::LearningController> controller2 = CreateObject<ns3::ofi::LearningController> ();
  if (!timeout.IsZero ()) controller2->SetAttribute ("ExpirationTime", TimeValue (timeout));
  switchHelper.Install (switchNode2, switch2Device, controller2);
  


  /* We enable OLSR (which will be consulted at a higher priority than
   * the global routing) on the backbone nodes
   */
  NS_LOG_INFO ("----------Enabling OLSR routing && Internet stack----------");
  
  OlsrHelper olsr;
  Ipv4StaticRoutingHelper staticRoute;
  Ipv4ListRoutingHelper list;

  /* list.Add(,0/10); 其中0或者10代表priority
   这里将 staticRoute 设置优先级 为0;
   olsr 设置优先级 为10;
  */

  list.Add (staticRoute, 0);
  list.Add (olsr, 10);

  /* Add internet stack to all the nodes, expect switches(交换机不用) */
  InternetStackHelper internet;
  internet.SetRoutingHelper (list); // has effect on the next Install ()
  internet.Install (csmaNodes);
  internet.Install (staWifi1Nodes);
  internet.Install (staWifi2Nodes);
  internet.Install (staWifi3Nodes);

  
  NS_LOG_INFO ("-----------Assigning IP Addresses.-----------");

  Ipv4AddressHelper ipCSMA, ipWIFI;
  ipCSMA.SetBase ("10.0.0.0",    "255.255.255.0");
  ipWIFI.SetBase ("192.168.0.0", "255.255.255.0");

  Ipv4InterfaceContainer h1h2Interface;
  Ipv4InterfaceContainer stasWifi1Interface;
  Ipv4InterfaceContainer stasWifi2Interface;
  Ipv4InterfaceContainer stasWifi3Interface;

  // AP 的地址池， WIFI和CSMA的
  Ipv4InterfaceContainer apWifi1Interface, apWifi2Interface, apWifi3Interface;
  Ipv4InterfaceContainer ap1CsmaInterface, ap2CsmaInterface, ap3CsmaInterface;

  /////////// ip for csma /////////////
  ap1CsmaInterface   = ipCSMA.Assign (ap1CsmaDevice); // 10.0.0.1 
  ap2CsmaInterface   = ipCSMA.Assign (ap2CsmaDevice); // 10.0.0.2
  ap3CsmaInterface   = ipCSMA.Assign (ap3CsmaDevice); // 10.0.0.3

  h1h2Interface      = ipCSMA.Assign (hostsDevice);   // 10.0.0.4~5

  // 共三个AP
  apWifi1Interface =   ipWIFI.Assign (apWifi1Device);  // 192.168.0.1
  apWifi2Interface =   ipWIFI.Assign (apWifi2Device);  // 192.168.0.2
  apWifi3Interface =   ipWIFI.Assign (apWifi3Device);  // 192.168.0.3

  // 供三组STA
  stasWifi1Interface = ipWIFI.Assign (stasWifi1Device); // 192.168.0.4~6
  stasWifi2Interface = ipWIFI.Assign (stasWifi2Device); // 192.168.0.7~10
  stasWifi3Interface = ipWIFI.Assign (stasWifi3Device); // 192.168.0.11



  NS_LOG_INFO ("-----------Enabling Static Routing.-----------");
  /**
   * Ipv4StaticRouting::SetDefaultRoute()
   * 
   * Add a default route to the static routing table.
   * This method tells the routing system what to do 
   * in the case where a specific route to a destination is not found. 
   * The system forwards packets to the specified node in the hope 
   * that it knows better how to route the packet.
   * 
   * @param: 
   *        metric: 度量值。是跟一组参数有关，包括带宽，通信代价，延迟，跳数，负载，路径成本和可靠性
   *                这个值越小 就会越优先选择
   */

  /* -----for StaticRouting(its very useful)----- */
  Ptr<Ipv4> h2Ip = hostsNode.Get(1)->GetObject<Ipv4> ();    // or csmaNodes.Get(4)
  // for node 14
  Ptr<Ipv4> ap3Ip = apsNode.Get(2)->GetObject<Ipv4> ();
  Ptr<Ipv4> sta1Wifi3Ip = staWifi3Nodes.Get(0)->GetObject<Ipv4> ();
  // for node 10
  //Ptr<Ipv4> ap2Ip = apsNode.Get(1)->GetObject<Ipv4> ();
  //Ptr<Ipv4> sta1Wifi2Ip = staWifi2Nodes.Get(0)->GetObject<Ipv4> ();

  /* the intermedia AP3 */
  //Ptr<Ipv4StaticRouting> staticRoutingAp3 = staticRoute.GetStaticRouting (Ap3Ip);
  //staticRoutingAp3->SetDefaultRoute(h1h2Interface.GetAddress(1), 1);
  //staticRoutingAp3->SetDefaultRoute(stasWifi3Interface.GetAddress(0), 1);

  /* --- the server  --- */
  Ptr<Ipv4StaticRouting> h2StaticRouting = staticRoute.GetStaticRouting (h2Ip);
  // for node 14 ---将 CSMA网络中的 H2 的默认下一跳为CSMA网络中的AP3----CSMA网卡IP
  h2StaticRouting->SetDefaultRoute(ap3CsmaInterface.GetAddress(0), 1);
  // for node 10 ---将 CSMA网络中的 H2 的默认下一跳为CSMA网络中的AP2----CSMA网卡IP
  //h2StaticRouting->SetDefaultRoute(ap2CsmaInterface.GetAddress(0), 1);
  
  /* --- the client  --- */
  // for node 14  ---将 WIFI#3 中的 STA1 的默认下一跳为其所在WIFI#3网络的AP3----WIFI网卡IP
  Ptr<Ipv4StaticRouting> sta1Wifi3StaticRouting = staticRoute.GetStaticRouting (sta1Wifi3Ip); // when node 14
  sta1Wifi3StaticRouting->SetDefaultRoute(apWifi3Interface.GetAddress(0), 1);
  // for node 10  ---将 WIFI#2 中的 STA1 的默认下一跳为其所在WIFI#3网络的AP2----WIFI网卡IP
  //Ptr<Ipv4StaticRouting> sta1Wifi2StaticRouting = staticRoute.GetStaticRouting (sta1Wifi2Ip); // when node 10
  //sta1Wifi2StaticRouting->SetDefaultRoute(apWifi2Interface.GetAddress(0), 1);


  NS_LOG_INFO ("-----------Creating Applications.-----------");
  uint16_t port = 9;   // Discard port (RFC 863)
  
  

  /* UDP server */
  UdpServerHelper server (port);  // for the server side, only one param(port) is specified
  // for node 6
  ApplicationContainer serverApps = server.Install (hostsNode.Get(1));
  serverApps.Start (Seconds(1.0));  
  serverApps.Stop (Seconds(stopTime));  
  

  /* UDP client */
  UdpClientHelper client (h1h2Interface.GetAddress(1) ,port);
  client.SetAttribute ("MaxPackets", UintegerValue (nMaxPackets));
  client.SetAttribute ("Interval", TimeValue (Seconds(nInterval)));  
  client.SetAttribute ("PacketSize", UintegerValue (nPacketSize));
  // for node 14
  ApplicationContainer clientApps = client.Install(staWifi3Nodes.Get(0));
  // for node 10
  //ApplicationContainer clientApps = client.Install(staWifi2Nodes.Get(0));
  // for node 5
  //ApplicationContainer clientApps = client.Install(hostsNode.Get(0));
  clientApps.Start (Seconds(1.1));  
  clientApps.Stop (Seconds(stopTime));
  


  /*
  // TCP server
  PacketSinkHelper sink ("ns3::TcpSocketFactory",
                         InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApps = sink.Install (hostsNode.Get(1));
  sinkApps.Start (Seconds (1.0));
  sinkApps.Stop (TimeValue(Seconds(stopTime)));


  // TCP client
  BulkSendHelper source ("ns3::TcpSocketFactory",
                         InetSocketAddress (h1h2Interface.GetAddress(1), port));
  // Set the amount of data to send in bytes.  Zero is unlimited.
  source.SetAttribute ("MaxBytes", UintegerValue (nMaxBytes));
  ApplicationContainer sourceApps = source.Install (staWifi3Nodes.Get(0));
  sourceApps.Start (Seconds (2.0));
  sourceApps.Stop (Seconds (10.0));
  */


  
  /** GlobalRouting does NOT work with Wi-Fi.
   * https://groups.google.com/forum/#!searchin/ns-3-users/wifi$20global$20routing/ns-3-users/Z9K1YrEmbcI/MrP2k47HAQAJ
   */
  //Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  NS_LOG_INFO ("-----------Configuring Tracing.-----------");

  /**
   * Configure tracing of all enqueue, dequeue, and NetDevice receive events.
   * Trace output will be sent to the file as below
   */
  if (tracing)
    {
      AsciiTraceHelper ascii;
      //csma.EnablePcapAll("goal-topo");
      csma.EnableAsciiAll (ascii.CreateFileStream ("goal-topo/goal-topo.tr"));
      wifiPhy.EnablePcap ("goal-topo/goal-topo-ap1-wifi", apWifi1Device);
      wifiPhy.EnablePcap ("goal-topo/goal-topo-ap2-wifi", apWifi2Device);
      wifiPhy.EnablePcap ("goal-topo/goal-topo-ap2-sta1-wifi", stasWifi2Device);
      wifiPhy.EnablePcap ("goal-topo/goal-topo-ap3-wifi", apWifi3Device);
      wifiPhy.EnablePcap ("goal-topo/goal-topo-ap3-sta1-wifi", stasWifi3Device);
      // WifiMacHelper doesnot have `EnablePcap()` method
      csma.EnablePcap ("goal-topo/goal-topo-switch1-csma", switch1Device);
      csma.EnablePcap ("goal-topo/goal-topo-switch2-csma", switch2Device);
      csma.EnablePcap ("goal-topo/goal-topo-ap1-csma", ap1CsmaDevice);
      csma.EnablePcap ("goal-topo/goal-topo-ap2-csma", ap2CsmaDevice);
      csma.EnablePcap ("goal-topo/goal-topo-ap3-csma", ap3CsmaDevice);
      csma.EnablePcap ("goal-topo/goal-topo-H1-csma", hostsDevice.Get(0));
      csma.EnablePcap ("goal-topo/goal-topo-H2-csma", hostsDevice.Get(1));
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

  AnimationInterface anim ("goal-topo/goal-topo.xml");
  anim.SetConstantPosition(switchNode1,30,0);             // s1-----node 0
  anim.SetConstantPosition(switchNode2,65,0);             // s2-----node 1
  anim.SetConstantPosition(apsNode.Get(0),5,20);      // Ap1----node 2
  anim.SetConstantPosition(apsNode.Get(1),30,20);      // Ap2----node 3
  anim.SetConstantPosition(apsNode.Get(2),55,20);      // Ap3----node 4
  anim.SetConstantPosition(hostsNode.Get(0),65,20);    // H1-----node 5
  anim.SetConstantPosition(hostsNode.Get(1),75,20);    // H2-----node 6
  //anim.SetConstantPosition(staWifi3Nodes.Get(0),55,40);  //   -----node 14

  anim.EnablePacketMetadata();   // to see the details of each packet



  NS_LOG_INFO ("------------Preparing for Check all the params.------------");
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop (Seconds(stopTime));
/*----------------------------------------------------------------------*/
  
  std::string base = "goal-topo-SDN__";
  //Throughput
  std::string throu = base + "ThroughputVSTime";
  std::string graphicsFileName        = throu + ".png";
  std::string plotFileName            = throu + ".plt";
  std::string plotTitle               = "Throughput vs Time";
  std::string dataTitle               = "Throughput";
  Gnuplot gnuplot (graphicsFileName);
  gnuplot.SetTitle (plotTitle);
  gnuplot.SetTerminal ("png");
  gnuplot.SetLegend ("Time", "Throughput");
  Gnuplot2dDataset dataset;
  dataset.SetTitle (dataTitle);
  dataset.SetStyle (Gnuplot2dDataset::LINES_POINTS);
  //Delay
  std::string delay = base + "DelayVSTime";
  std::string graphicsFileName1        = delay + ".png";
  std::string plotFileName1            = delay + ".plt";
  std::string plotTitle1               = "Delay vs Time";
  std::string dataTitle1               = "Delay";
  Gnuplot gnuplot1 (graphicsFileName1);
  gnuplot1.SetTitle (plotTitle1);
  gnuplot1.SetTerminal ("png");
  gnuplot1.SetLegend ("Time", "Delay");
  Gnuplot2dDataset dataset1;
  dataset1.SetTitle (dataTitle1);
  dataset1.SetStyle (Gnuplot2dDataset::LINES_POINTS);
  //LostPackets
  std::string lost = base + "LostPacketsVSTime";
  std::string graphicsFileName2        = lost + ".png";
  std::string plotFileName2            = lost + ".plt";
  std::string plotTitle2               = "LostPackets vs Time";
  std::string dataTitle2               = "LostPackets";
  Gnuplot gnuplot2 (graphicsFileName2);
  gnuplot2.SetTitle (plotTitle2);
  gnuplot2.SetTerminal ("png");
  gnuplot2.SetLegend ("Time", "LostPackets");
  Gnuplot2dDataset dataset2;
  dataset2.SetTitle (dataTitle2);
  dataset2.SetStyle (Gnuplot2dDataset::LINES_POINTS);
  //Jitter
  std::string jitter = base + "JitterVSTime";
  std::string graphicsFileName3        = jitter + ".png";
  std::string plotFileName3            = jitter + ".plt";
  std::string plotTitle3               = "Jitter vs Time";
  std::string dataTitle3               = "Jitter";
  Gnuplot gnuplot3 (graphicsFileName3);
  gnuplot3.SetTitle (plotTitle3);
  gnuplot3.SetTerminal ("png");
  gnuplot3.SetLegend ("Time", "Jitter");
  Gnuplot2dDataset dataset3;
  dataset3.SetTitle (dataTitle3);
  dataset3.SetStyle (Gnuplot2dDataset::LINES_POINTS);

/*-----------------------------------------------------*/
  // 测吞吐量, 延时, 丢包, 抖动, 最后打印出这些参数
  ThroughputMonitor (&flowmon, monitor, dataset);
  DelayMonitor      (&flowmon, monitor, dataset1);
  LostPacketsMonitor(&flowmon, monitor, dataset2);
  JitterMonitor     (&flowmon, monitor, dataset3);
  PrintParams       (&flowmon, monitor);
/*-----------------------------------------------------*/


  NS_LOG_INFO ("------------Running Simulation.------------");
  Simulator::Run ();

  //Throughput
  gnuplot.AddDataset (dataset);
  std::ofstream plotFile (plotFileName.c_str());
  gnuplot.GenerateOutput (plotFile);
  plotFile.close ();
  //Delay
  gnuplot1.AddDataset (dataset1);
  std::ofstream plotFile1 (plotFileName1.c_str());
  gnuplot1.GenerateOutput (plotFile1);
  plotFile1.close ();
  //LostPackets
  gnuplot2.AddDataset (dataset2);
  std::ofstream plotFile2 (plotFileName2.c_str());
  gnuplot2.GenerateOutput (plotFile2);
  plotFile2.close ();
  //Jitter
  gnuplot3.AddDataset (dataset3);
  std::ofstream plotFile3 (plotFileName3.c_str());
  gnuplot3.GenerateOutput (plotFile3);
  plotFile3.close ();


  monitor->SerializeToXmlFile("goal-topo/goal-topo.flowmon", true, true);
  /* the SerializeToXmlFile () function 2nd and 3rd parameters 
   * are used respectively to activate/deactivate the histograms and the per-probe detailed stats.
   */
  Simulator::Destroy ();
}
