#!/bin/sh
. "$(dirname "$0")/env.sh"
exec qemu-system-aarch64 -M virt -cpu max -m 512 -nographic \
  -kernel "$KDIR/arch/arm64/boot/Image" -initrd "$INITRAMFS" \
  -netdev user,id=net0,hostfwd=tcp::5020-:502 \
  -device virtio-net-device,netdev=net0 \
  -append "console=ttyAMA0 rdinit=/init"
