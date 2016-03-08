klem
====

Wireless 802.11 Linux Kernel Link Emulation

(Last tested on Kernel 3.19.0.52 for Ubutnu 14.03)

KLEM is a virtual wireless 802.11 network driver that connects devices together using Kernel Link EMulation.

The Linux kernel since version 2.6.32 has a SoftMAC 802.11 framework known as mac80211 that allows wireless frame management to be done completely in software.  The KLEM device driver uses this framework and wraps the generated 802.11 frames into a raw Ethernet packet and broadcasts them over a wired network.   Each virtual or physical machine connected to the wired network that receives the raw KLEM packet can strip out the wireless packet and process it as if that packet was received from an actual radio.

The KLEM driver wraps each wireless 802.11 frame with the following specialized raw Ethernet packet format:o

     Length     Description             Value

     6 bytes    Destination MAC         0xffffffffffff (Broadcast)
     6 bytes    Source MAC              MAC of virtual wireless device
     2 bytes    Protocol                0xdead
     4 bytes    Header                  “klem”
     4 bytes    Version                 1	
     4 bytes    Band                    Band used to “transmit” packet
     4 bytes    Frequency               Frequency used to “transmit” packet
     4 bytes    Power                   Power level used to “transmit” packet.
     4 bytes    KLEM ID                 value between 0-255 

Usage
-----

The default 802.11 network simulation used by KLEM would simulate an environment where every wireless node is within range of each other.   On each virtual or physical machine, type the following to load KLEM and begin an ad-hoc 802.11 wireless link emulation.

     #modprobe mac80211
     #modprobe cfg80211
     #insmod klem.ko
     #echo “device = eth0” > /proc/klem
     #echo “command = start” > /proc/klem
     #/sbin/iwconfig wlan0 mode ad-hoc
     #/sbin/iwconfig wlan0 essid 2
     #/sbin/ifconfig wlan0 x.x.x.x

     x.x.x.x represents the IP address assigned to wlan0 on the virtual or 
             physical machine.
     eth0    represents the wired Ethernet connection to transmit the raw
             klem packets.

Once each virtual or physical machine is set up, you should be able to ping 
each of them.
 
Alternative network deployments can be accomplished by setting the numerical KLEM ID in each virtual machine.  In a mesh network configuration you need to set an ID for each virtual or physical machine on the network and you can also add a filter to prevent certain KLEM ID’s from directly communicating with each other, forcing them to hop between other KLEM connected machines.

     #modprobe mac80211
     #modprobe cfg80211
     #insmod klem.ko
     #echo “device = eth0” > /proc/klem

     Each mesh can be assigned a specific ID
     #echo “id = 10” > /proc/klem

     Each mesh can be specified to filter certain ID’s from being received,
     thus allowing specialized virtual topologies to be represented.
     #echo “filter = 30” > /proc/klem

     #echo “command = start” > /proc/klem
     #iw dev wlan0 interface add mesh type mp mesh_id loki
     #ifconfig mesh x.x.x.x/24


Build
-----

KLEM was built and tested using Fedora Core 15 19 and Ubutnu 14.03.   It is assumed that the kernel development package has been installed onto the system.  In Fedora Core, the kernel development package is installed in /usr/src/kernels/`uname –r` directory.

The KLEM driver also requires the same gcc compiler used to build the kernel be installed, along with the makefile packages.

To build KLEM, type make from within its release directory.

    #cd klem/release
    #make

The klem.ko driver will be built in this directory.

If your kernel sources are in an alternative directory, setting the KERNELDIR variable will indicate to make where to look for the required files.

    #make KERNELDIR=/usr/foo/kernel




