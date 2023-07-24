#!/bin/bash

ip link add name br0 type bridge
ip link set dev br0 up

iptables -P INPUT ACCEPT
iptables -P FORWARD ACCEPT
iptables -P OUTPUT ACCEPT
iptables -t nat -F
iptables -t mangle -F
iptables -F
iptables -X

echo "1" > /proc/sys/net/ipv4/ip_forward

