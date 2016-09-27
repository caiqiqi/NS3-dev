# Some notes I made about ns-3

## MaxPackets in ns-3

Reference: https://groups.google.com/forum/#!topic/ns-3-users/KFL0pckjEw4

`MaxPackets`这个值只是说在一次Simulation过程中，能发送的最大包的个数。但是如果你的Simulation时间过短，而Interval过大，导致并不能在Simulation过程中发送那么多包。要使能在Simulation过程中发送那么多包，则要保证有足够多的时间做Simulation，才能发送那么多包。
Q:</br>
Im a new ns-3 user. i have created a simple topology and now im sending packets from one node to another. there are three attributes for traffic, i-e- Max Packets, Interval, and Packet Size in the udp-echo-cliend. When i change the MaxPackets from one value to another and run the simulation, same number of packets are sent. the number of packets send doesnot vary.

Can anyone tell me how can i change the max number of packets sent by udp-echo-client?

A:</br>
The max number of packets, refer to the total packets that are available to be scheduled by the UDP application.
If you simulate a scenario for 10 seconds and you have an interval 1sec, you are going to see maximum 10 packets during the simulation, even if you have set the maxPackets>10. They just don't have the time to be transmitted.

If you want to see more packets in your simulation, increase the interval. Make sure that the total "available" packets (MaxPackets) are more than simulation_time/interval.