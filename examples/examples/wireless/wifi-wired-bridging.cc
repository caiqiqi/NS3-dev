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

//
// Default network topology includes some number of AP nodes specified by
// the variable nWifis (defaults to two).  Off of each AP node, there are some
// number of STA nodes specified by the variable nStas (defaults to two).
// Each AP talks to its associated STA nodes.  There are bridge net devices
// on each AP node that bridge the whole thing into one network.
//
//      +-----+      +-----+            +-----+      +-----+
//      | STA |      | STA |            | STA |      | STA | 
//      +-----+      +-----+            +-----+      +-----+
//    192.168.0.2  192.168.0.3        192.168.0.5  192.168.0.6
//      --------     --------           --------     --------
//      WIFI STA     WIFI STA           WIFI STA     WIFI STA
//      --------     --------           --------     --------
//        ((*))       ((*))       |      ((*))        ((*))
//                                |
//              ((*))             |             ((*))
//             -------                         -------
//             WIFI AP   CSMA ========= CSMA   WIFI AP 
//             -------   ----           ----   -------
//             ##############           ##############
//                 BRIDGE                   BRIDGE
//             ##############           ############## 
//               192.168.0.1              192.168.0.4
//               +---------+              +---------+
//               | AP Node |              | AP Node |
//               +---------+              +---------+
//

#include "ns3/core-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/bridge-helper.h"
#include <vector>
#include <stdint.h>
#include <sstream>
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("wifiWiredBridgingScript");

int main (int argc, char *argv[])
{
  uint32_t nWifis = 2;
  uint32_t nStas = 2;
  bool sendIp = true;
  bool writeMobility = false;

  CommandLine cmd;
  cmd.AddValue ("nWifis", "Number of wifi networks", nWifis);
  cmd.AddValue ("nStas", "Number of stations per wifi network", nStas);
  cmd.AddValue ("SendIp", "Send Ipv4 or raw packets", sendIp);
  cmd.AddValue ("writeMobility", "Write mobility trace", writeMobility);
  cmd.Parse (argc, argv);

  NodeContainer backboneNodes;
  NetDeviceContainer backboneDevices;
  Ipv4InterfaceContainer backboneInterfaces;
  /*
   * 向量vector属于标准模板库，可以广义上认为是数组的增强版
   * 与数组相比其优点在于能根据需要随时自动调整自身的大小以便容下所要放的元素
   * 一下几个vector只声明了，但是没有初始化其大小
   * */
  std::vector<NodeContainer> staNodes;
  std::vector<NetDeviceContainer> staDevices;
  std::vector<NetDeviceContainer> apDevices;
  std::vector<Ipv4InterfaceContainer> staInterfaces;
  std::vector<Ipv4InterfaceContainer> apInterfaces;

  InternetStackHelper stack;
  CsmaHelper csma;
  Ipv4AddressHelper ip;
  ip.SetBase ("192.168.0.0", "255.255.255.0");

  backboneNodes.Create (nWifis);
  stack.Install (backboneNodes);

  backboneDevices = csma.Install (backboneNodes);

  double wifiX = 0.0;

  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO); 

  for (uint32_t i = 0; i < nWifis; ++i)
    {
      // calculate ssid for wifi subnetwork
      std::ostringstream oss;
      oss << "wifi-default-" << i;
      Ssid ssid = Ssid (oss.str ());

      NodeContainer sta;
      NetDeviceContainer staDev;
      NetDeviceContainer apDev;
      Ipv4InterfaceContainer staInterface;
      Ipv4InterfaceContainer apInterface;
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
      mobility.Install (backboneNodes.Get (i));
      wifiMac.SetType ("ns3::ApWifiMac",
                       "Ssid", SsidValue (ssid));
      apDev = wifi.Install (wifiPhy, wifiMac, backboneNodes.Get (i));

      NetDeviceContainer bridgeDev;
      /* 
       * `ns3::BridgeHelper::Install(Ptr<Node> node, NetDeviceContainer c)`
       * creates an `ns3::BridgeNetDevice` with the attributes configured by
       * `BridgeHelper::SetDeviceAttribute()`, adds the device to the node,
       * and attachs the given NetDevices as ports of the bridge.
       * @param node: The node to install the device in
       * @param c: Container of NetDevices to add as bridge posrts
       * returns : A container holding the added net device.
       */
      bridgeDev = bridge.Install (backboneNodes.Get (i), NetDeviceContainer (apDev, backboneDevices.Get (i)));
      //以上把骨干网上的索引为i的ap的网卡和这里的apDev网卡加到骨干网的这个索引为i的节点上，即第i+1个AP上

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

  NS_LOG_INFO ("-----Creating Applications.-----");
  uint16_t port = 9;

  UdpEchoServerHelper echoServer (port);
  ApplicationContainer serverApps = echoServer.Install (staNodes[1].Get (1));   // server is 1,1
  serverApps.Start (Seconds(1.0));
  serverApps.Stop  (Seconds(5.0));

  UdpEchoClientHelper echoClient (staInterfaces[1].GetAddress(1) ,port);  // dest: ip, port
  echoClient.SetAttribute ("MaxPackets", UintegerValue (4));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
  ApplicationContainer clientApps = echoClient.Install(staNodes[0].Get (0));  // client is 0,0
  clientApps.Start (Seconds(2.0));
  clientApps.Stop (Seconds(5.0));


  wifiPhy.EnablePcap ("wifi-wired-bridging", apDevices[0]);
  wifiPhy.EnablePcap ("wifi-wired-bridging", apDevices[1]);

  if (writeMobility)
    {
      AsciiTraceHelper ascii;
      MobilityHelper::EnableAsciiAll (ascii.CreateFileStream ("wifi-wired-bridging.mob"));
    }

  Simulator::Stop (Seconds (5.0));
  Simulator::Run ();
  Simulator::Destroy ();
}
