klem
====

Wireless 802.11 Linux Kernel Link Emulation

KLEM is a virtual wireless 802.11 network driver that connects devices together using Kernel Link EMulation.

Started using this example driver to test mesh networking.   Added scripts to setup bridge interface and to start the virtal machines.

Parallels MacBook Pro M1
====

Currently using a ubuntu host on an Apple MacBook Pro M1 to run these qemu sessions.  Probably a bit recursive from a virtual stack point of view.

./startbr0.sh

This script opens up iptables, creates br0 and allows traffic to route

./startvm.sh tapX

Starts the qemu images contained in vmlinux-3.18.99 and initramfs.cpio.tar.gz.

MacBook Pro M1 Host OS
====

Created a startup script for Apple MacBook Pro M1

Still had to execute it as root.

sudo ./startm1.sh
