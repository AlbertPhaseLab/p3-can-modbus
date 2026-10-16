#!/bin/sh
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
. "$HERE/env.sh"

echo "== Copiando modulos CAN del kernel =="
cp "$KDIR/drivers/net/can/dev/can-dev.ko" "$ROOTFS/root/"
cp "$KDIR/net/can/can.ko" "$ROOTFS/root/"
cp "$KDIR/drivers/net/can/vcan.ko" "$ROOTFS/root/"

echo "== Compilando can-utils (estatico) =="
cd "$CANUTILS_DIR"
make clean >/dev/null 2>&1 || true
make CC=aarch64-linux-gnu-gcc CFLAGS="-static" LDFLAGS="-static" candump cansend cangen cansniffer

echo "== Copiando binarios de can-utils =="
cp candump cansend cangen cansniffer "$ROOTFS/root/" 2>/dev/null || cp candump cansend "$ROOTFS/root/"

echo "== Repackeando initramfs =="
cd "$ROOTFS"
find . | cpio -o -H newc --owner root:root | gzip > "$INITRAMFS"
echo "OK: initramfs actualizado"

echo "== Compilando can_reader =="
cd "$HERE/src"
aarch64-linux-gnu-gcc -static can_reader.c -o can_reader
cp can_reader "$ROOTFS/root/"
