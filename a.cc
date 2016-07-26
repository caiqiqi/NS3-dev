#include <ns3/core-module.h>
#include <ns3/internet-module.h>
#include <ns3/csma-module.h>
#include <ns3/bridge-module.h>
#include <ns3/applications-module.h>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Project");

ns3::Time timeout = ns3::Seconds (0);

int main(int argc, char *argv[])
{

  NS_LOG_INFO("Create Nodes");
  NodeContainer csmanode;
  csmanode.Create(4);

  NodeContainer OFSwitch;
  OFSwitch.Create(1);

  NS_LOG_INFO("Build Topology");
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", DataRateValue (5000000));
  csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));

  NetDeviceContainer csmaNetDevices, csmaNetDevices1, csmaNetDevices2, link;
  NetDeviceContainer switchDevice;

  //connect node0 to node1
  link = csma.Install(NodeContainer(csmanode.Get(0),csmanode.Get(1)));
  csmaNetDevices.Add(link.Get(0));
  csmaNetDevices.Add(link.Get(1));

  //connect node1 to OFSwitch
  link = csma.Install(NodeContainer(csmanode.Get(1),OFSwitch.Get(0)));
  csmaNetDevices1.Add(link.Get(0));
  switchDevice.Add(link.Get(1));

  //connect OFSwitch to node2
  link = csma.Install(NodeContainer(OFSwitch.Get(0),csmanode.Get(2)));
  switchDevice.Add(link.Get(0));
  csmaNetDevices1.Add(link.Get(1));

//  link = csma.Install(NodeContainer(csmanode.Get(1),csmanode.Get(2)));
//  csmaNetDevices1.Add(link.Get(0));
//  csmaNetDevices1.Add(link.Get(1));

  //connect node2 to node3
  link = csma.Install(NodeContainer(csmanode.Get(2),csmanode.Get(3)));
  csmaNetDevices2.Add(link.Get(0));
  csmaNetDevices2.Add(link.Get(1));


  //Create Switch net device, which will do packet switching

  Ptr<Node> switchNode = OFSwitch.Get (0);
  BridgeHelper bridge;
  std::cout << "Switched NetDevices " << switchDevice.GetN () << std::endl;
  bridge.Install (switchNode, switchDevice);

  //Add Internet stack to terminals
  InternetStackHelper internet;
  RipHelper rip;
  internet.SetRoutingHelper (rip);
  internet.Install(csmanode);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  address.Assign (csmaNetDevices);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  address.Assign (csmaNetDevices1);

  address.SetBase ("10.1.3.0", "255.255.255.0");
  address.Assign (csmaNetDevices2);

  // Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  //Print IP address of Nodes
  for (int i=0; i<4; i++)
    {
      Ptr<Node> n = csmanode.Get (i);
      Ptr<Ipv4> ipv4 = n->GetObject<Ipv4> ();
      for (uint32_t j=0; j<ipv4->GetNInterfaces(); j++)
        {
          Ipv4InterfaceAddress ipv4_int_addr = ipv4->GetAddress (j, 0);
          Ipv4Address ip_addr = ipv4_int_addr.GetLocal ();
          std::cout << "Node: " << i << " IP Address " << j << ": " << ip_addr << std::endl;
          NS_LOG_INFO ("Node: " << i << " IP Address " << j << ": " << ip_addr);
        }
    }

  Ptr<Node> n = csmanode.Get (0);
  Ptr<Ipv4> ipv4 = n->GetObject<Ipv4> ();
  Ipv4InterfaceAddress ipv4_int_addr = ipv4->GetAddress (1, 0);
  Ipv4Address ip_addr = ipv4_int_addr.GetLocal ();

  Ptr<OutputStreamWrapper> routintable = Create<OutputStreamWrapper>("routingtable",std::ios::out);
  Ipv4GlobalRoutingHelper ipv4global;
  ipv4global.PrintRoutingTableAllAt(Seconds(50.0), routintable);



  OnOffHelper onoff("ns3::UdpSocketFactory",Address(InetSocketAddress(ip_addr,1024)));
  onoff.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  onoff.SetAttribute ("DataRate", DataRateValue (DataRate (6000)));

  ApplicationContainer apps = onoff.Install(csmanode.Get(3));
  apps.Start(Seconds(10.0));
  apps.Stop(Seconds(20.0));


  PacketSinkHelper sink ("ns3::UdpSocketFactory", Address (InetSocketAddress (Ipv4Address::GetAny(), 1024)));
  apps = sink.Install(csmanode.Get(0));
  apps.Start(Seconds(0.0));
  apps.Stop(Seconds(22.0));


  AsciiTraceHelper ascii;
  Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream ("MobilityWlans.tr");
  csma.EnableAsciiAll (stream);
  csma.EnablePcapAll ("MobilityWlans", false);



  NS_LOG_INFO ("Run Simulation.");
  Simulator::Stop (Seconds (100));
  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");

}

















