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
cp candump cansend cangen cansniffer "$ROOTFS/root/" 2>/dev/null || cp candump cansend "$ROOTFS/root/"

echo "== Compilando programas propios =="
cd "$HERE/src"
for p in can_reader can_sender; do
  aarch64-linux-gnu-gcc -static "$p.c" -o "$p"
  cp "$p" "$ROOTFS/root/"
done
for p in modbus_slave modbus_master gateway; do
  aarch64-linux-gnu-gcc -static "$p.c" -o "$p" \
    -I"$MODBUS_PREFIX/include/modbus" -L"$MODBUS_PREFIX/lib" -lmodbus
  cp "$p" "$ROOTFS/root/"
done

cp "$HERE/rootfs-extra/p3-setup.sh" "$ROOTFS/root/"
cp "$HERE/rootfs-extra/p3-setup.sh" "$ROOTFS/root/"
echo "== Repackeando initramfs =="
cd "$ROOTFS"
find . | cpio -o -H newc --owner root:root | gzip > "$INITRAMFS"
echo "OK: initramfs actualizado"
