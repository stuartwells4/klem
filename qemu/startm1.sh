EMAC=`printf 'DE:AD:BE:EF:%02X:%02X\n' $((RANDOM%256)) $((RANDOM%256))`

qemu-system-aarch64 -machine virt -cpu cortex-a57 -smp 1 -m 1G \
		-global virtio-blk-device.scsi=off \
		-device virtio-scsi-device,id=scsi \
		-append "console=ttyAMA0 root=/dev/sda oops=panic panic_on_warn=1 panic=-1 ftace_dump_on_oops=orig_cpu debug slub_debug=UZ" \
		-netdev vmnet-host,id=unet,net-uuid=8493bb04-56aa-4ec0-a937-605ac1a8bd07 \
		-device virtio-net-device,netdev=unet,mac=$EMAC \
		-nographic \
		-serial mon:stdio \
		-kernel vmlinuz-3.18.99 -initrd initramfs.cpio.gz
