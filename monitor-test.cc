/*
 * myglobalrouting.cc
 *
 *  Created on: Jan 10, 2012
 *      Author: yeison
 */

//
// Network topology
//
//  n0
//     \ 5 Mb/s, 2ms
//      \          1.5Mb/s, 10ms
//       n2 -------------------------n3
//      /
//     / 5 Mb/s, 2ms
//   n1
//
// - all links are point-to-point links with indicated one-way BW/delay
// - CBR/UDP flows from n0 to n3, and from n3 to n1
// - FTP/TCP flow from n0 to n3, starting at time 1.2 to time 1.35 sec.
// - UDP packet size of 210 bytes, with per-packet interval 0.00375 sec.
//   (i.e., DataRate of 448,000 bps)
// - DropTail queues
// - Tracing of queues and packet receptions to file "simple-global-routing.tr"

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/gnuplot.h"


using namespace ns3;

NS_LOG_COMPONENT_DEFINE("monitor-test");

const std::string MONITOR_TEST = "monitor-test" ;
//用于测吞吐量
double ThroughputMonitor(FlowMonitorHelper *fmhelper, Ptr<FlowMonitor> flowMon, Gnuplot2dDataset DataSet);

int
main (int argc, char** argv)
{
	LogComponentEnable("monitor-test", LOG_LEVEL_INFO);
	Config::SetDefault ("ns3::OnOffApplication::PacketSize", UintegerValue (210));
	Config::SetDefault ("ns3::OnOffApplication::DataRate", StringValue ("448kb/s"));

	CommandLine cmd;
	bool enableFlowMonitor = false;
	cmd.AddValue("EnableFlowMonitor", "Enable Flow Monitor", enableFlowMonitor);
	cmd.Parse(argc, argv);

	NS_LOG_INFO("Creating the nodes");
	NodeContainer c;
	c.Create(4);
	NodeContainer n0n2 = NodeContainer (c.Get(0), c.Get(2));
	NodeContainer n1n2 = NodeContainer (c.Get(1), c.Get(2));
	NodeContainer n3n2 = NodeContainer (c.Get(3), c.Get(2));

	InternetStackHelper internet;
	internet.Install(c);

	NS_LOG_INFO("Creating channels and adding address space");

	PointToPointHelper p2p;
	p2p.SetChannelAttribute("Delay", StringValue("2ms"));
	p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));

	NetDeviceContainer d0d2 = p2p.Install(n0n2);
	NetDeviceContainer d1d2= p2p.Install(n1n2);

	p2p.SetDeviceAttribute ("DataRate", StringValue ("1500kbps"));
	p2p.SetChannelAttribute ("Delay", StringValue ("10ms"));

	NetDeviceContainer d3d2 = p2p.Install(n3n2);

	//Address space

	Ipv4AddressHelper ipv4;
	ipv4.SetBase("10.1.1.0", "255.255.255.0");
	Ipv4InterfaceContainer i0i2 = ipv4.Assign(d0d2);

	ipv4.SetBase("10.1.2.0", "255.255.255.0");
	Ipv4InterfaceContainer i1i2 = ipv4.Assign(d1d2);

	ipv4.SetBase("10.1.3.0", "255.255.255.0");
	Ipv4InterfaceContainer i3i2 = ipv4.Assign(d3d2);

	Ipv4GlobalRoutingHelper::PopulateRoutingTables();

	// Create the OnOff application to send UDP datagrams of size
	// **210** bytes at a rate of **448 Kb/s**

	NS_LOG_INFO("Create applications");
	uint16_t port = 9; // Discard port (RFC 863)

	OnOffHelper onoff ("ns3::UdpSocketFactory", Address (InetSocketAddress (i3i2.GetAddress(0), port))); // (the address of the remote node to send traffic to. in this case n3)
	onoff.SetAttribute("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]") );
	onoff.SetAttribute("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]") );

	ApplicationContainer apps = onoff.Install(c.Get(0)); //install in n0
	apps.Start(Seconds(1.0));
	apps.Stop(Seconds(10.0));

	// Create a packet sink to receive these packets (in n3)

	PacketSinkHelper sink ("ns3::UdpSocketFactory", Address(InetSocketAddress(Ipv4Address::GetAny(), port)));
	apps = sink.Install(c.Get(3));
	apps.Start(Seconds(1.0));
	apps.Stop(Seconds(10.0));

	// Create a similar flow from n3 to n1, starting at time 1.1 seconds

	onoff.SetAttribute("Remote", AddressValue (InetSocketAddress (i1i2.GetAddress(0), port)));
	apps = onoff.Install(c.Get(3));
	apps.Start(Seconds(1.1));
	apps.Stop(Seconds(10.0));

	//Create a packet sink to receive the packets

	apps = sink.Install(c.Get(1));

	// 抓包到.pcap
	p2p.EnablePcapAll(MONITOR_TEST, true);


	std::string fileNameWithNoExtension = "plot-2d";
	std::string graphicsFileName        = fileNameWithNoExtension + ".png";
	std::string plotFileName            = fileNameWithNoExtension + ".plt";
	std::string plotTitle               = "2-D Plot";
	std::string dataTitle               = "2-D Data";

	// Instantiate the plot and set its title.
	Gnuplot plot (graphicsFileName);
	plot.SetTitle (plotTitle);

	// Make the graphics file, which the plot file will create when it is used with Gnuplot, be a `PNG` file.
	plot.SetTerminal ("png");

	// Set the labels for each axis.
	plot.SetLegend("Flow", "Throughput");
	// Set the range for the x axis.(-6 to +6)
	plot.AppendExtra ("set xrange [-6:+6]");

	// Instantiate the dataset, set its title, and make the points be
	// plotted along with connecting lines.
	Gnuplot2dDataset dataset;
	dataset.SetTitle (dataTitle);
	dataset.SetStyle (Gnuplot2dDataset::LINES_POINTS);

	//如果enableFlowMonitor设置为true了，则开启监控模式
	Ptr<FlowMonitor> flowMon;
	FlowMonitorHelper flowMonHelper;
	double throu;
	if (enableFlowMonitor)
	{
		flowMon = flowMonHelper.InstallAll();
		// 调用吞吐量监控
		throu = ThroughputMonitor(&flowMonHelper, flowMon, dataset);
	}

    
    flowMon->SerializeToXmlFile( "flowMonitor" + MONITOR_TEST + ".xml", true, true);
    // update gnuplot data
    dataset.Add((double) Simulator::Now().GetSeconds(), throu); // 根据从模拟开始的时间作为横轴x，吞吐量作为纵轴y
    std::cout << "---------------------------------------------------------------------------" << std::endl;

	Simulator::Stop(Seconds(11));

	AnimationInterface animation(MONITOR_TEST + ".xml");
	animation.EnablePacketMetadata(false);

	Simulator::Run();

	// !!! 注意一定要在run()之后再AddDataset() !!!
	//Gnuplot ...continued
    plot.AddDataset(dataset);
    // Open the plot file.
    std::ofstream plotFile(plotFileName.c_str());
    // Write the plot file.
    plot.GenerateOutput(plotFile);
    // Close the plot file.
    plotFile.close();
	Simulator::Destroy();

	return 0;

}

double ThroughputMonitor(FlowMonitorHelper *fmhelper, Ptr<FlowMonitor> flowMon, Gnuplot2dDataset DataSet) {
    
    double localThrou = 0;
    std::map<FlowId, FlowMonitor::FlowStats> flowStats = flowMon->GetFlowStats();
    Ptr<Ipv4FlowClassifier> classing = DynamicCast<Ipv4FlowClassifier> (fmhelper->GetClassifier());
    // 下面是计算吞吐量的算法
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator stats = flowStats.begin(); stats != flowStats.end(); ++stats) {
        /*
        Classifies packets by looking at their IP and TCP/UDP headers. 
        FiveTuple五元组是：(source-ip, destination-ip, protocol, source-port, destination-port)
        */
        Ipv4FlowClassifier::FiveTuple fiveTuple = classing->FindFlow(stats->first);
        std::cout << "Flow ID     : " << stats->first << " ; " << fiveTuple.sourceAddress << " -----> " << fiveTuple.destinationAddress << std::endl;
        std::cout << "Tx Packets = " << stats->second.txPackets << std::endl;
        std::cout << "Rx Packets = " << stats->second.rxPackets << std::endl;
        std::cout << "Duration    : " << (stats->second.timeLastRxPacket.GetSeconds() - stats->second.timeFirstTxPacket.GetSeconds()) << std::endl;
        std::cout << "Last Received Packet  : " << stats->second.timeLastRxPacket.GetSeconds() << " Seconds" << std::endl;
        std::cout << "Throughput: " << stats->second.rxBytes * 8.0 / (stats->second.timeLastRxPacket.GetSeconds() - stats->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024 << " Mbps" << std::endl;
        
        localThrou = (stats->second.rxBytes * 8.0 / (stats->second.timeLastRxPacket.GetSeconds() - stats->second.timeFirstTxPacket.GetSeconds()) / 1024 / 1024);
    }

    return localThrou;

}