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
  uint32_t nSwitch     = 2;  // switch的数量
  uint32_t nAp         = 3;  // AP的数量
  uint32_t nTerminal   = 2;  // 不移动的终端节点数量
  uint32_t nStaAp[3]   = {3, 4, 1};  // 把各个wifi网络的sta数放在一个数组里面
  //uint32_t nAp1Station = nStaAp[0];
  //uint32_t nAp2Station = nStaAp[1];
  //uint32_t nAp3Station = nStaAp[2];

  ns3::Time stopTime = ns3::Seconds (5.0);

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

  WifiHelper            wifi;
  WifiMacHelper         wifiMac;
  YansWifiPhyHelper     wifiPhy = YansWifiPhyHelper::Default();
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();

  wifiPhy.SetChannel (wifiChannel.Create ());
  //wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);

  // `BridgeHelper` : Add capability to bridge multiple LAN segments (IEEE 802.1D bridging)
  BridgeHelper bridge;
  
  InternetStackHelper stack ;
  CsmaHelper csma ;
  Ipv4AddressHelper ip;
  ip.SetBase ("192.168.0.0", "255.255.255.0" );




  NS_LOG_INFO ("-----Creating nodes-----");
 
  // setup swtiches (switch节点=> #0 #1)
  NodeContainer switchesNodes;
  switchesNodes.Create (nSwitch); 
  // setup AP (AP节点=> #2 #3 #4)
  NodeContainer apBackboneNodes;                              // AP节点，共 #3个
  apBackboneNodes.Create (nAp);
  NetDeviceContainer apCsmaDevices;                           // AP的骨干网中的csma网卡设备
  NetDeviceContainer apWifiDevices;                           // AP的wifi网中的wifi网卡设备
  std::vector<NetDeviceContainer> vec_bridgeDev ;
  std::vector<Ipv4InterfaceContainer> vec_apInterfaces(3);    // AP的ip地址池  
  std::vector<NetDeviceContainer> vec_staDevices(3);          // sta的wifi网卡设备，共 #3组
  std::vector<Ipv4InterfaceContainer> vec_staInterfaces(3);   // sta的地址池，共 #3组

  /* 
   * `ns3::BridgeHelper::Install(Ptr<Node> node, NetDeviceContainer c)`
   * creates an `ns3::BridgeNetDevice` with the attributes configured by
   * `BridgeHelper::SetDeviceAttribute()`, adds the device to the node,
   * and attachs the given NetDevices as ports of the bridge.
   * @param node: The node to install the device in
   * @param c: Container of NetDevices to add as bridge posrts
   * returns : A container holding the added net device.
  */


  for (uint32_t i=0; i< nAp; ++i)
  {
    apWifiDevices.Get(i) = wifi.Install (wifiPhy, wifiMac, apBackboneNodes.Get(i));
    // 把骨干网上的索引为i的AP的网卡和这里的apWifiDevices.Get(i)网卡加到骨干网的这个索引为i的节点上，即第i+1个AP上
    vec_bridgeDev[i] = bridge.Install (apBackboneNodes.Get (i), NetDeviceContainer (apWifiDevices.Get(i), apCsmaDevices.Get (i) ) );
    // assign AP IP address to bridge, not wifi
    vec_apInterfaces[i] = ip.Assign (vec_bridgeDev[i]);
  }

  

  // setup terminales (终端节点> #5 #6)
  NodeContainer terminalsNodes;
  terminalsNodes.Create (nTerminal);
  // setup sta (sta节点=> [#7 #8 #9], [#10 #11 #12 #13], [#14] )
  std::vector<NodeContainer> vec_staNodes(3);                 // sta节点，共 #3组
  ///用for循环创建nAp组sta节点，每组的个数不一样，为 nStaAp[i]
  for (uint32_t i=0; i< nAp; ++i)
  {
    vec_staNodes[i].Create (nStaAp[i]);
  }
 

  apBackboneNodes.Create (nAp) ;   // 创建nAp个AP节点
  stack.Install (apBackboneNodes) ;
  apCsmaDevices = csma.Install (apBackboneNodes); // 给AP节点安装csma网络，得到其csma网卡设备

  

  /* 不移动的节点， 称它为 "stable_mobility" */
  MobilityHelper stable_mobility;
  // We want the AP to remain in a fixed position during the simulation
  stable_mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  stable_mobility.Install (apBackboneNodes);   // AP是不动的
  stable_mobility.Install (vec_staNodes[2]);   // 第#3组的sta是不动的
  stable_mobility.Install (terminalsNodes);    // 两个终端节点也是不动的
  stable_mobility.Install (switchesNodes);     // switch也是不动的


  /* 有几个节点是不动的，暂且称它为 "moving_mobility" 
     所有AP，两个终端节点，
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

  
  for (uint32_t i = 0; i < nAps; ++i)
    {
      // 计算wifi子网的ssid
      std::ostringstream oss;
      oss << "wifi-default-" << i;
      Ssid ssid = Ssid (oss.str ());

      if (i==0)
      {

      }
      MobilityHelper mobility;
      // `BridgeHelper` : Add capability to bridge multiple LAN segments (IEEE 802.1D bridging)
      BridgeHelper bridge;
      WifiHelper wifi;
      WifiMacHelper wifiMac;
      YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
      wifiPhy.SetChannel (wifiChannel.Create ());

      sta.Create (nStas);
      mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                     "MinX", DoubleValue (wifiX),
                                     "MinY", DoubleValue (0.0),
                                     "DeltaX", DoubleValue (5.0),
                                     "DeltaY", DoubleValue (5.0),
                                     "GridWidth", UintegerValue (1),
                                     "LayoutType", StringValue ("RowFirst"));


      // setup the AP.
      mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
      mobility.Install (apBackboneNodes.Get (i));
      wifiMac.SetType ("ns3::ApWifiMac",
                       "Ssid", SsidValue (ssid));
      apDev = wifi.Install (wifiPhy, wifiMac, apBackboneNodes.Get (i));

      // assign AP IP address to bridge, not wifi
      apInterface = ip.Assign (bridgeDev);

      // setup the STAs
      stack.Install (sta);
      mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                                 "Mode", StringValue ("Time"),
                                 "Time", StringValue ("2s"),
                                 "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"),
                                 "Bounds", RectangleValue (Rectangle (wifiX, wifiX+5.0,0.0, (nStas+1)*5.0)));
      mobility.Install (sta);
      wifiMac.SetType ("ns3::StaWifiMac",
                       "Ssid", SsidValue (ssid));
      staDev = wifi.Install (wifiPhy, wifiMac, sta);
      staInterface = ip.Assign (staDev);

      // save everything in containers.
      // push_back() : 在vector尾部加入一个数据
      staNodes.push_back (sta);
      apDevices.push_back (apDev);
      apInterfaces.push_back (apInterface);
      staDevices.push_back (staDev);
      staInterfaces.push_back (staInterface);

      wifiX += 20.0;
    }



  NS_LOG_INFO ("-----Running Simulation.-----");
  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_INFO ("-----Done.-----");
  #else
  NS_LOG_INFO ("-----NS-3 OpenFlow is not enabled. Cannot run simulation.-----");
  #endif // NS3_OPENFLOW
}
