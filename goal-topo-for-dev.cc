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
#include "ns3/olsr-helper.h"


#include "ns3/netanim-module.h"

using namespace ns3;

// 用于命令行操作 `$ export NS_LOG=GoalTopoForDevelopmentScript=info`
NS_LOG_COMPONENT_DEFINE ("GoalTopoForDevelopmentScript");


bool verbose = false;
bool tracing  = true;


/*
 * 是否显示详细信息
*/
bool
SetVerbose (std::string value)
{
  verbose = true;
  return true;
}


int
main (int argc, char *argv[])
{
  uint32_t nSwitch     = 2;              // switch的数量
  uint32_t nAp         = 3;              // AP的数量
  uint32_t nTerminal   = 2;              // 不移动的终端节点数量
  uint32_t nStaAp[3]   = {3, 4, 1};      // 把各个wifi网络的sta数放在一个数组里面
  //uint32_t nAp1Station = nStaAp[0];
  //uint32_t nAp2Station = nStaAp[1];
  //uint32_t nAp3Station = nStaAp[2];


  ns3::Time stopTime = ns3::Seconds (10.0);

  //这里初始化Ssid动态数组的值，方便后面好迭代。
  std::vector<Ssid> ssid ;
  //error: in C++98 ‘ssid’ must be initialized by constructor, not by ‘{...}’
  // = { Ssid ("Ssid-AP1"), Ssid ("Ssid-AP2"), Ssid ("Ssid-AP3") } ; 
  ssid[0] = Ssid("Ssid-AP1") ;
  ssid[1] = Ssid("Ssid-AP2") ;
  ssid[2] = Ssid("Ssid-AP3") ;


  #ifdef NS3_OPENFLOW


  CommandLine cmd;

  cmd.AddValue ("v", "Verbose (turns on logging).", MakeCallback (&SetVerbose));
  cmd.AddValue ("verbose", "Verbose (turns on logging).", MakeCallback (&SetVerbose));

  cmd.Parse (argc, argv);

  if (verbose)
    {
      // LogComponentEnable ("OpenFlowCsmaSwitch", LOG_LEVEL_INFO);
      // LogComponentEnable ("OpenFlowInterface", LOG_LEVEL_INFO);
      // LogComponentEnable ("OpenFlowSwitchNetDevice", LOG_LEVEL_INFO);
      // LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);  
      // LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO); 
    }

  

//////////////////////////////////////////
/////  NetWork Helper    /////////////////
//////////////////////////////////////////

  WifiHelper            wifi;
  //NqosWifiMacHelper     wifiMac;  // for ns-3.24
  WifiMacHelper         wifiMac;    // for ns-3.25
  YansWifiPhyHelper     wifiPhy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifi.SetRemoteStationManager ("ns3::AarfWifiManager");
  wifiPhy.SetChannel (wifiChannel.Create ());
  //wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);

  // `BridgeHelper` : Add capability to bridge multiple LAN segments (IEEE 802.1D bridging)
  BridgeHelper bridge;
  
  InternetStackHelper internet ;
  CsmaHelper csma ;
  
  Ipv4AddressHelper ip_csma;
  ip_csma.SetBase ("192.168.0.0", "255.255.255.0") ;
  // AP#1:  192.168.0.1
  // AP#2:  192.168.0.2
  // AP#3:  192.168.0.3
  // 终端#1: 192.168.0.4
  // 终端#2: 192.168.0.5

  Ipv4AddressHelper ip_wifi;
  ip_wifi.SetBase ("10.0.0.0", "255.255.255.0") ;
  // AP#1:   10.0.0.1
  // AP#2:   10.0.0.2
  // AP#3:   10.0.0.3
  // wifi#1: 10.0.0.7-9
  // wifi#2: 10.0.0.10-13
  // wifi#3: 10.0.0.14




  NS_LOG_INFO ("----------------------Creating nodes--------------------");
  
  ////////////// 网络节点  /////////////////
  
  //  #0 #1 => switch节点
  NodeContainer switchesNodes;
  switchesNodes.Create (nSwitch); 
  //  #2 #3 #4 => AP节点
  NodeContainer apBackboneNodes;                              // AP节点，共 #3个
  apBackboneNodes.Create (nAp);                               // 创建nAp个AP节点
  //  #5 #6 => 终端节点
  NodeContainer terminalsNodes;
  terminalsNodes.Create (nTerminal);
  // [#7 #8 #9], [#10 #11 #12 #13], [#14] => sta节点
  std::vector<NodeContainer> vec_staNodes(3);                 // sta节点，共 #3组
  ///用for循环创建nAp组sta节点，每组的个数不一样，为 nStaAp[i]
  for (uint32_t i=0; i< nAp; ++i)
  {
    vec_staNodes[i].Create (nStaAp[i]);
  }


  ////////////// 网卡设备  ///////////////////
  std::vector<NetDeviceContainer> vec_apCsmaDevices(3);       // AP的骨干网中的csma网卡设备,由于每个AP也都要连接好几个网卡设备，所以也为vector
  std::vector<NetDeviceContainer> vec_apWifiDevices(3);       // AP的wifi网中的wifi网卡设备
  std::vector<NetDeviceContainer> vec_bridgeDevices(3) ;      // 用于分配ip的桥接网卡设备  PS: 3为nAp的值
  std::vector<NetDeviceContainer> vec_staDevices(3);          // sta的wifi网卡设备，共 #3组
  std::vector<NetDeviceContainer> vec_terminalsDevices(2);    // terminal节点的csma网卡
  std::vector<NetDeviceContainer> vec_switchesDevices(2);     // 两个switch的csma网卡,由于每个switch的网卡都要连接好几个设备，所以这里得用一个vector
  
  /////////////// IP地址池  /////////////////
  std::vector<Ipv4InterfaceContainer> vec_csma_apInterfaces(3);           // AP的csma网络ip地址池
  std::vector<Ipv4InterfaceContainer> vec_csma_terminalInterfaces(2);     // 终端的csma网络ip池
  std::vector<Ipv4InterfaceContainer> vec_wifi_apInterfaces(3);           // AP的wifi网络ip地址池  
  std::vector<Ipv4InterfaceContainer> vec_wifi_staInterfaces(3);          // sta的wifi网络ip地址池，共 #3组

  // 给第i个AP节点安装csma，得到第i个节点的csma网卡
  for (uint32_t i=0; i< nAp; ++i)
  {
    vec_apCsmaDevices[i] = csma.Install (apBackboneNodes.Get(i));
  }
  // 给第i个终端节点安装csma，得到第i个终端节点的csma网卡
  for (uint32_t i=0; i<nTerminal; ++i)
  {
    vec_terminalsDevices[i] = csma.Install (terminalsNodes.Get(i));
  }




//////////////////////////////////////////////
/////// 给各个节点设置Mobility模型 //////////////
//////////////////////////////////////////////

  /* 不移动的节点， 称它为 "stable_mobility" */
  MobilityHelper stable_mobility;
  // We want the AP to remain in a fixed position during the simulation
  stable_mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  stable_mobility.Install (apBackboneNodes);   // AP是不动的
  stable_mobility.Install (vec_staNodes[2]);   // 第#3组的sta是不动的
  stable_mobility.Install (terminalsNodes);    // 两个终端节点也是不动的
  stable_mobility.Install (switchesNodes);     // switch也是不动的


  /* 有几个节点是移动的，暂且称它为 "moving_mobility" 
  */
  MobilityHelper  moving_mobility;
  // 给第#1组sta设置mobility
  moving_mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
    "MinX",      DoubleValue (0),
    "MinY",      DoubleValue (25),
    "DeltaX",    DoubleValue (5),
    "DeltaY",    DoubleValue (5),
    "GridWidth", UintegerValue(3),
    "LayoutType",StringValue ("RowFirst")
    );    // "GridWidth", UintegerValue(3),
  moving_mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel", 
    "Bounds", RectangleValue (Rectangle (-50, 50, -50, 50)));
  moving_mobility.Install (vec_staNodes[0]);

  // 给第#2组sta设置mobility
  moving_mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
    "MinX",      DoubleValue (25),
    "MinY",      DoubleValue (25),
    "DeltaX",    DoubleValue (5),
    "DeltaY",    DoubleValue (5),
    "GridWidth", UintegerValue(3),
    "LayoutType",StringValue ("RowFirst")
    );    // "GridWidth", UintegerValue(3),
  moving_mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel", 
    "Bounds", RectangleValue (Rectangle (-50, 50, -50, 50)));
  moving_mobility.Install (vec_staNodes[1]);




////////////////////////////////////////////////////////////
///////////-------switch1与switch2-----------////////////////
///////////-------3个AP与switch1---------////////////////////
///////////-------2个终端与switch2////////////////////////////
//////////--------2个controller分别与2个switch////////////////
////////////////////////////////////////////////////////////
  NS_LOG_INFO ("-----Building Topology------");
  
  NetDeviceContainer link;

  //Connect ofSwitch1 to ofSwitch2  
  link = csma.Install(NodeContainer(switchesNodes.Get(0),switchesNodes.Get(1)));    // switch1 和switch2
  // 将link的第一个元素给第一个switch的网卡， 将link的第二个元素给第二个switch的网卡
  for (uint32_t i = 0; i < nSwitch; ++i)
  {
    vec_switchesDevices[i].Add( link.Get(i) );
  }
  
  //Connect AP1, AP2 and AP3 to ofSwitch1  
  // link is a list, including the two nodes
  for (uint32_t i = 0; i < nAp; ++i)
  {
      link = csma.Install (NodeContainer (apBackboneNodes.Get(i), switchesNodes.Get(0)) );  // switch1
      // 将link的第一个元素加到第i个AP节点的csma网卡container中
      vec_apCsmaDevices[i].Add( link.Get(0) );
      // 将link的第二个元素加到第1个switch网卡的container中
      vec_switchesDevices[0].Add( link.Get(1));
  }

  //Connect terminal1 and terminal2 to ofSwitch2 
  for (uint32_t i = 0; i < nTerminal; i++)
  {
    /* 给终端节点和switch2组成的NodeContainer安装csma，然后终端的csma卡加入这一网卡。
    * 将link.Get(0)给 第i+1个终端的csma网卡Container
    * 将link.Get(1)给 switch2的csma网卡Container
    */
    link = csma.Install( NodeContainer(terminalsNodes.Get(i), switchesNodes.Get(1)) );   // switch2
    vec_terminalsDevices[i].Add( link.Get(0) );
    vec_switchesDevices[1].Add( link.Get(1) );
  }




  /* 
   * for OpenFlow Controller
   * 两个controller分别与两个switch
  */
  OpenFlowSwitchHelper switchHelper;

  Ptr<ns3::ofi::LearningController> controller = CreateObject<ns3::ofi::LearningController> ();
  switchHelper.Install (switchesNodes.Get(0), vec_switchesDevices[0], controller);
  //switchHelper.Install (switchNode2, switch2Device, controller);
  Ptr<ns3::ofi::LearningController> controller2 = CreateObject<ns3::ofi::LearningController> ();
  switchHelper.Install (switchesNodes.Get(1), vec_switchesDevices[1], controller2);



///////////////////////////////////////////////////////////////////////////
/////////////////////////// Internet and IP address assigning /////////////
//////////////////////////////////////////////////////////////////////////
  internet.Install (apBackboneNodes) ;
  internet.Install (terminalsNodes)  ;
  internet.Install (vec_staNodes[0]) ;
  internet.Install (vec_staNodes[1]) ;
  internet.Install (vec_staNodes[2]) ;



/////////////////////////////////////////////////////////////////////////////
/////////////////// ---------------- `192.168.0.0/24`--------------//////////
/////////////////////////////////////////////////////////////////////////////
  // 先依次给AP的csma网卡分配
  // ip: 192.168.0.1-3
  for (uint32_t i = 0; i < nAp; ++i)
  {
    vec_csma_apInterfaces[i] =  ip_csma.Assign (vec_apCsmaDevices[i]);
  }
  // 再依次两个终端节点分配
  // ip: 192.168.0.4-5
  for (uint32_t i = 0; i < nTerminal; ++i)
  {
    vec_csma_terminalInterfaces[i] =  ip_csma.Assign (vec_terminalsDevices[i]);
  }

/////////////////////////////////////////////////////////////////////////////
/////////////////// ---------------- `10.0.0.0/24`--------------//////////
/////////////////////////////////////////////////////////////////////////////

  /* 
   * `ns3::BridgeHelper::Install(Ptr<Node> node, NetDeviceContainer c)`
   *
   * creates an `ns3::BridgeNetDevice` with the attributes configured by
   * `BridgeHelper::SetDeviceAttribute()`, adds the device to the node,
   * and attachs the given NetDevices as ports of the bridge.
   * @param node: The node to install the device in
   * @param c: Container of NetDevices to add as bridge posrts
   * returns : A container holding the added net device.
  */
  
  for (uint32_t i=0; i< nAp; ++i)
  {
    wifiMac.SetType ("ns3::ApWifiMac", "Ssid", SsidValue(ssid[i]) );
    vec_apWifiDevices[i] = wifi.Install (wifiPhy, wifiMac, apBackboneNodes.Get(i));
    // 把骨干网上的索引为i的AP的csma网卡和这里的vec_apWifiDevices.Get(i)的wifi网卡加到骨干网的这个索引为i的节点上，即第i+1个AP上
    vec_bridgeDevices[i] = bridge.Install (apBackboneNodes.Get (i), NetDeviceContainer (vec_apWifiDevices[i], vec_apCsmaDevices[i] ) );
    // 把给AP的ip给bridge, 而不是wifi
    vec_wifi_apInterfaces[i] = ip_wifi.Assign (vec_bridgeDevices[i]);
  }


///// 给sta节点分配其所属的 SSID，安装wifi协议，分配IP地址

  for (uint32_t i = 0; i < nAp; ++i)
  {
      wifiMac.SetType ("ns3::StaWifiMac", "Ssid", SsidValue(ssid[i]), "ActiveProbing", BooleanValue (false) );
      vec_staDevices[i] = wifi.Install (wifiPhy, wifiMac, vec_staNodes[i] );
      vec_wifi_staInterfaces[i] = ip_wifi.Assign (vec_staDevices[i] );

  }


  // Add applications
  NS_LOG_INFO ("----------------------Creating Applications.-------------------");
  
  ///////////// Create one udpServer application on node one.////////////////
  uint16_t port = 8000;
  UdpServerHelper udpServer (port);

  ApplicationContainer server_apps;
  server_apps = udpServer.Install (terminalsNodes.Get(1));   // node #6 (192.168.0.x) (第二个固定终端节点) 作为 UdpServer

  server_apps.Start (Seconds (1.0));
  server_apps.Stop (Seconds (10.0));


  /////////////// Create one UdpClient application to send UDP datagrams///////

  // 这个interfaces的第2个元素，然后得到它的第一个ip地址(其实只有一个ip) 
  UdpClientHelper client (vec_csma_terminalInterfaces[1].GetAddress(0), port);        // dest: IP,port

  client.SetAttribute ("MaxPackets", UintegerValue (320));       // 最大数据包数
  client.SetAttribute ("Interval", TimeValue (Time ("0.5")));   // 时间间隔 0.01太小了吧，包不会太多了吗
  client.SetAttribute ("PacketSize", UintegerValue (1024));      // 包大小

  ApplicationContainer client_apps;
  client_apps = client.Install (vec_staNodes[0].Get (2));    // node #9 (10.0.0.x)  也就是AP1下的9号节点

  client_apps.Start (Seconds (2.0));
  client_apps.Stop (Seconds (10.0));

  // 设置Simulator停止的时间
  Simulator::Stop (stopTime);


  NS_LOG_INFO ("---------------------Configuring Tracing.-----------------------");
  
  if (tracing)
  {
    AsciiTraceHelper ascii;
    //csma.EnablePcapAll("goal-topo");
    csma.EnableAsciiAll (ascii.CreateFileStream ("goal-topo.tr"));
    /*    `YansWifiPhyHelper`的 Enablepcap()方法，第二个参数可接收NetDeviceContainer，也可以为Ptr<NetDevice>，还可以是  NodeContainer    */
    wifiPhy.EnablePcap ("goal-topo-ap1-wifi", vec_bridgeDevices[0]); 
    wifiPhy.EnablePcap ("goal-topo-ap2-wifi", vec_bridgeDevices[1]);
    wifiPhy.EnablePcap ("goal-topo-ap3-wifi", vec_bridgeDevices[2]);
    wifiPhy.EnablePcap ("goal-topo-ap1-sta3-wifi", vec_staDevices[0].Get(2));    // 这里因为是让ap1里面的sta3作为UdpClient( #9 节点)，所以要记录它的流量
    // WifiMacHelper doesnot have `EnablePcap()` method
    csma.EnablePcap ("goal-topo-switch1-csma", vec_switchesDevices[0]);           // switch1的csma网卡
    csma.EnablePcap ("goal-topo-switch2-csma", vec_switchesDevices[1]);           // switch2的csma网卡
    csma.EnablePcap ("goal-topo-ap1-csma", vec_apCsmaDevices[0]);
    csma.EnablePcap ("goal-topo-ap2-csma", vec_apCsmaDevices[1]);
    csma.EnablePcap ("goal-topo-ap3-csma", vec_apCsmaDevices[2]);
    csma.EnablePcap ("goal-topo-H1-csma", vec_terminalsDevices[0] );
    csma.EnablePcap ("goal-topo-H2-csma", vec_terminalsDevices[1] );
  }



  NS_LOG_INFO ("----------------------Running Simulation.-----------------------");
  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_INFO ("--------------------------Done.---------------------------------");
  #else
  NS_LOG_INFO ("-----NS-3 OpenFlow is not enabled. Cannot run simulation.-------");
  #endif // NS3_OPENFLOW
}
