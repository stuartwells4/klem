klem
====

Wireless 802.11 Linux Kernel Link Emulation

(Builds on Ubuntu 24.04 kernels 6.8, 6.11, 6.14 and 6.17 and on Ubuntu 22.04 kernel 5.15. Kernel 5.15 is the oldest supported version.)

KLEM is a virtual wireless 802.11 network driver that connects devices together using Kernel Link EMulation.

The Linux kernel since version 2.6.32 has a SoftMAC 802.11 framework known as mac80211 that allows wireless frame management to be done completely in software.  The KLEM device driver uses this framework and wraps the generated 802.11 frames in raw Ethernet packets and broadcasts them over a wired network.   Each virtual or physical machine connected to the wired network that receives the raw KLEM packet can strip out the wireless packet and process it as if that packet was received from an actual radio.

KLEM versus mac80211_hwsim
--------------------------

To simulate a wireless network on a single machine, use the kernel's built-in mac80211_hwsim driver instead.  It creates any number of virtual radios in one kernel and passes frames between them internally, and each radio can be moved into its own container or network namespace.

KLEM is for emulating a wireless network across multiple hosts.  It provides one virtual radio per kernel and carries the 802.11 frames between physical or virtual machines over a wired Ethernet network.  Containers on the same host share one kernel, so they cannot be separate KLEM nodes.

The KLEM driver wraps each wireless 802.11 frame with the following specialized raw Ethernet packet format:

     Length     Description             Value

     6 bytes    Destination MAC         0xffffffffffff (Broadcast)
     6 bytes    Source MAC              MAC of the sender’s wired interface (e.g. eth0)
     2 bytes    Protocol                0xdead
     4 bytes    Header                  “melk” (0x6d656c6b)
     4 bytes    Version                 1
     4 bytes    Band                    Band used to “transmit” packet
     4 bytes    Frequency               Frequency used to “transmit” packet
     4 bytes    Power                   Power level used to “transmit” packet
     4 bytes    KLEM ID                 Value between 0 and 255

Usage
-----

The default 802.11 network simulation used by KLEM simulates an environment where every wireless node is within range of every other node.   On each virtual or physical machine, type the following to load KLEM and begin an ad-hoc 802.11 wireless link emulation.

     #modprobe mac80211
     #modprobe cfg80211
     #insmod klem.ko
     #echo "device = eth0" > /proc/klem
     #echo "command = start" > /proc/klem
     #/sbin/iwconfig wlan0 mode ad-hoc
     #/sbin/iwconfig wlan0 essid 2
     #/sbin/ifconfig wlan0 x.x.x.x

     x.x.x.x represents the IP address assigned to wlan0 on the virtual or 
             physical machine.
     eth0    represents the wired Ethernet connection to transmit the raw
             klem packets.

The start and stop commands finish in the background.  Read /proc/klem to see
the result: the status line shows starting, then running, or failed with a
reason such as "no device set".

     #grep status /proc/klem
     status:               running

Once each virtual or physical machine is set up, you should be able to ping 
each of them.
 
Alternative network deployments can be accomplished by setting the numerical KLEM ID in each virtual machine.  In a mesh network configuration you need to set an ID for each virtual or physical machine on the network, and you can also add a filter to prevent certain KLEM IDs from directly communicating with each other, forcing them to hop through other KLEM-connected machines.

     #modprobe mac80211
     #modprobe cfg80211
     #insmod klem.ko
     #echo "device = eth0" > /proc/klem

     Each mesh node can be assigned a specific ID
     #echo "id = 10" > /proc/klem

     Each mesh node can filter out certain IDs, allowing specialized
     virtual topologies to be represented.
     #echo "filter = 30" > /proc/klem

     The mode is lemu (the default) or bridge.  Set it before starting;
     changing it while running needs a stop and start to take effect.
     #echo "mode = lemu" > /proc/klem

     #echo "command = start" > /proc/klem
     #iw dev wlan0 interface add mesh type mp mesh_id loki
     #ifconfig mesh x.x.x.x/24


Build
-----

KLEM was built and tested using Fedora Core 15 and 19 and Ubuntu 14.04.   It is assumed that the kernel development package has been installed onto the system.  In Fedora Core, the kernel development package is installed in the /usr/src/kernels/`uname -r` directory.

The KLEM driver also requires that the same gcc compiler used to build the kernel be installed, along with make.

To build KLEM, type make from within its release directory.

    #cd klem/release
    #make

The klem.ko driver will be built in this directory.

If your kernel sources are in an alternative directory, setting the KERNELDIR variable tells make where to look for the required files.

    #make KERNELDIR=/usr/foo/kernel

On Ubuntu 24.04 install the build prerequisites first:

    #sudo apt install build-essential linux-headers-$(uname -r)

The iwconfig/ifconfig commands used above come from the wireless-tools and
net-tools packages, which are no longer installed by default.  The iw and ip
equivalents work as well, for example:

    #iw dev wlan0 set type ibss
    #ip link set wlan0 up
    #iw dev wlan0 ibss join klem 2412
    #ip addr add x.x.x.x/24 dev wlan0

If Secure Boot is enabled, the kernel will refuse to load an unsigned klem.ko.
Either sign the module with a key enrolled through mokutil, or disable Secure
Boot validation.

Raspberry Pi
------------

On Raspberry Pi OS, install the headers for the running kernel, then build as
above:

    #sudo apt install build-essential linux-headers-$(uname -r)

mac80211 names the KLEM radio wlan0 only if no other wireless device exists.
The onboard Wi-Fi normally takes wlan0, so disable it to give that name to
KLEM.  Add this line to /boot/firmware/config.txt and reboot:

    dtoverlay=disable-wifi

Check the name with "iw dev" after starting KLEM.

KLEM allows one station and one access point interface up at the same time,
sharing one channel, as the onboard brcmfmac radio does.  Each interface gets
its own MAC address, so both can be added to batman-adv:

    #iw dev wlan0 interface add wlan1 type __ap




