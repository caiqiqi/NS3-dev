## The topology
- Node 0,1: the top two nodes in GRAY, representing two switches with openflow supported; stationary.
- Node 2,3,4: the three BLUE nodes in a line, representing three APs; stationary.
- Node 5,6: the ORANGE nodes, one of which act as the UDP(in current simulation) or TCP server; stationary.
- Node 7,8,9: the three RED nodes, are three STAs in association with node 2, within node 2's WIFI network; ; randomly walk.
- Node 10,11,12,13: the four RED nodes blow the node 3, are four STAs in association with node 3, within or out of node 3's WIFI network; randomly walk.
- Node 14: the ont RED node below the node 4, the only one STA in association with node 4, within  node 4's WIFI network; stationary.


Note that the black wires means wired link, and the dotted wires means wireless links. In WIFI networks, if the dotted wire exist between the STA and the AP, that means they have been associated. If not, that means they are not associated because the distance between the two are out of the range of the signal.

In PyViz, there are two options for users to display the traffic, "Selected node" and "All nodes". </br>
## Choose the "selected node" option
If we choose the option "Selected node", in the path from the source(node 10, the one with black circle) to the destination(node 6, the ORANGE one), we could only see the traffic sent by the selected node, i.e the source . As we can see in this picture, ![](img/selected-nodes-1.png) </br>
the link throughput from the RED node to the BLUE node is `848.00kbit/s`. Note that the direction the arrow suggests is the direction in which the traffci heads to. While the link throughput of the other three links are all the same, `856.00kbit/s`. This is because that the throughput in wired links are very stable, and in the simulation process, we did not find any packets loss in the wired links. </br>
Then, as the simulation time goes by, in the next picture
![](img/selected-nodes-2.png) </br>
we could see a throughput change, from `848.00kbit/s` to `850.56kbit/s`. This is because the source node is sending OLSR HNA messages to the nodes within its reach. While the wired link throughput remains. </br>
## Choose the "All nodes" option
Then we choose the "All nodes" option. Now we can see all the traffic sent by every node in the topology.
![](img/all-nodes.png)</br>
Now we can see that the link throughput from node 3 to node 0, from node 0 to node 1, and from node 1 to node 6 has changed. This is because there are traffic sent by other nodes and they are all join in together.