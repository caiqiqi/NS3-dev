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

// 用于命令行操作 `$ export NS_LOG=GoalTopoScript=info`
NS_LOG_COMPONENT_DEFINE ("GoalTopoScript");


bool verbose = false;
bool use_drop = false;
bool tracing  = true;
ns3::Time timeout = ns3::Seconds (0);

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


  ns3::Time stopTime = ns3::Seconds (5.0);

  //这里初始化Ssid动态数组的值，方便后面好迭代。
  std::vector<Ssid> ssid = { Ssid ("Ssid-AP1"), Ssid ("Ssid-AP2"), Ssid ("Ssid-AP3") } ;

  #ifdef NS3_OPENFLOW


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
  WifiMacHelper         wifiMac;
  YansWifiPhyHelper     wifiPhy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifi.SetRemoteStationManager ("ns3::AarfWifiManager");
  wifiPhy.SetChannel (wifiChannel.Create ());
  //wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);

  // `BridgeHelper` : Add capability to bridge multiple LAN segments (IEEE 802.1D bridging)
  BridgeHelper bridge;
  
  InternetStackHelper internet ;
  CsmaHelper csma ;
  Ipv4AddressHelper ip;
  ip.SetBase ("192.168.0.0", "255.255.255.0" );




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
  std::vector<NetDeviceContainer> vec_apCsmaDevices(2);       // AP的骨干网中的csma网卡设备,由于每个AP也都要连接好几个网卡设备，所以也为vector
  NetDeviceContainer apWifiDevices;                           // AP的wifi网中的wifi网卡设备
  std::vector<NetDeviceContainer> vec_bridgeDevices(3) ;      // 用于分配ip的桥接网卡设备  PS: 3为nAp的值
  std::vector<NetDeviceContainer> vec_staDevices(3);          // sta的wifi网卡设备，共 #3组
  std::vector<NetDeviceContainer> vec_terminalsDevices(2);    // terminal节点的csma网卡
  std::vector<NetDeviceContainer> vec_switchesDevices(2);     // 两个switch的csma网卡,由于每个switch的网卡都要连接好几个设备，所以这里得用一个vector
  
  /////////////// IP地址池  /////////////////
  std::vector<Ipv4InterfaceContainer> vec_apInterfaces(3);    // AP的ip地址池  
  std::vector<Ipv4InterfaceContainer> vec_staInterfaces(3);   // sta的ip地址池，共 #3组

  // 给第i个AP节点安装csma，得到第i个节点的csma网卡
  for (uint32_t i=0; i< nAp; ++i)
  {
    vec_apCsmaDevices.Get(i) = csma.Install (apBackboneNodes.Get(i));
  }
  // 给第i个终端节点安装csma，得到第i个终端节点的csma网卡
  for (uint32_t i=0; i<nTerminal; ++i)
  {
    vec_terminalsDevices.Get(i) = csma.Install (terminalsNodes.Get(i));
  }

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
    wifiMac.SetType ("ns3::ApWifiMac", "Ssid", ssid[i] );
    apWifiDevices.Get(i) = wifi.Install (wifiPhy, wifiMac, apBackboneNodes.Get(i));
    // 把骨干网上的索引为i的AP的网卡和这里的apWifiDevices.Get(i)网卡加到骨干网的这个索引为i的节点上，即第i+1个AP上
    vec_bridgeDevices[i] = bridge.Install (apBackboneNodes.Get (i), NetDeviceContainer (apWifiDevices.Get(i), apCsmaDevices.Get (i) ) );
    // 把给AP的ip给bridge, 而不是wifi
    vec_apInterfaces[i] = ip.Assign (vec_bridgeDevices[i]);
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



////////////////////////////////////////////////////
///// 给sta节点分配其所属的 SSID，安装wifi协议，分配IP地址
////////////////////////////////////////////////////

  for (uint32_t i = 0; i < nAps; ++i)
  {
      wifiMac.SetType ("ns3::StaWifiMac",
                       "Ssid", ssid[i],
                       "ActiveProbing", BooleanValue (false) );
      vec_staDevices[i] = wifi.Install (wifiPhy, wifiMac, vec_staNodes[i] );
      vec_staInterfaces[i] = ip.Assign (vec_staDevices[i] );

      // wifiX += 20.0;
  }

  //TODO 给终端节点安装IP



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
    vec_switchesDevices.Get(i).Add( link.Get(i) );
  }
  
  //Connect AP1, AP2 and AP3 to ofSwitch1  
  // link is a list, including the two nodes
  for (uint32_t i = 0; i < nAps; ++i)
  {
      link = csma.Install (NodeContainer (apBackboneNodes.Get(i), switchesNodes.Get(0)) );  // switch1
      // 将link的第一个元素加到第i个AP节点的csma网卡container中
      vec_apCsmaDevices.Get(i).Add( link.Get(0) );
      // 将link的第二个元素加到第1个switch网卡的container中
      vec_switchesDevices.Get(0).Add( link.Get(1));
  }

  //Connect terminal1 and terminal2 to ofSwitch2 
  for (int i = 0; i < nTerminal; i++)
  {
    /* 给终端节点和switch2组成的NodeContainer安装csma，然后终端的csma卡加入这一网卡。
    * 将link.Get(0)给 第i+1个终端的csma网卡Container
    * 将link.Get(1)给 switch2的csma网卡Container
    */
    link = csma.Install( NodeContainer(vec_terminalsDevices.Get(i), switchesNodes.Get(1)) );   // switch2
    vec_terminalsDevices.Get(i).Add( link.Get(0) );
    vec_switchesDevices.Get(1).Add( link.Get(1) );
  }




  /* 
   * for OpenFlow Controller
   * 两个controller分别与两个switch
  */
  OpenFlowSwitchHelper switchHelper;

  Ptr<ns3::ofi::LearningController> controller = CreateObject<ns3::ofi::LearningController> ();
  switchHelper.Install (switchNode1, switch1Device, controller);
  //switchHelper.Install (switchNode2, switch2Device, controller);
  Ptr<ns3::ofi::LearningController> controller2 = CreateObject<ns3::ofi::LearningController> ();
  switchHelper.Install (switchNode2, switch2Device, controller2);

///////////////////////////////////////////////////////////////////////////
/////////////////////////// Internet and IP address assigning /////////////
//////////////////////////////////////////////////////////////////////////
  internet.Install (apBackboneNodes) ;
  internet.Install (terminalsNodes)  ;
  internet.Install (vec_staNodes[0]) ;
  internet.Install (vec_staNodes[1]) ;
  internet.Install (vec_staNodes[2]) ;





  NS_LOG_INFO ("-----Running Simulation.-----");
  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_INFO ("-----Done.-----");
  #else
  NS_LOG_INFO ("-----NS-3 OpenFlow is not enabled. Cannot run simulation.-----");
  #endif // NS3_OPENFLOW
}
