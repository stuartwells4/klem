#!/bin/bash
EMAC=`printf 'DE:AD:BE:EF:%02X:%02X\n' $((RANDOM%256)) $((RANDOM%256))`

switch=br0

# tunctl -u `whoami` -t $1 (use ip tuntap instead!)
ip tuntap add $1 mode tap user `whoami`
ip link set $1 up
sleep 0.5s
# brctl addif $switch $3 (use ip link instead!)
ip link set $1 master $switch
    
qemu-system-aarch64 -machine virt -cpu cortex-a57 -smp 1 -m 1G \
		-global virtio-blk-device.scsi=off \
		-device virtio-scsi-device,id=scsi \
		-append "console=ttyAMA0 root=/dev/sda oops=panic panic_on_warn=1 panic=-1 ftace_dump_on_oops=orig_cpu debug slub_debug=UZ" \
		-netdev tap,ifname=$1,script=no,downscript=no,id=unet \
		-device virtio-net-device,netdev=unet,mac=$EMAC \
		-nographic \
		-serial mon:stdio \
		-kernel vmlinuz-3.18.99 -initrd initramfs.cpio.gz
 reset
 exit 0
