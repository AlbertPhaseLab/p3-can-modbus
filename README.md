# p3-can-modbus — Embedded Linux Portfolio, project 3/3

Linux + MCU-style gateway: SocketCAN (virtual CAN bus) and Modbus, running in
QEMU (`-M virt`, arm64) from WSL2. Reuses the kernel tree from
[p1-i2c](https://github.com/AlbertPhaseLab/p1-i2c), extended with
`CONFIG_CAN`/`CONFIG_CAN_VCAN`.

## Contents

- `env.sh`, `build.sh`, `run.sh` — build and boot scripts, following the same
  pattern as project 1. `build.sh` copies the CAN kernel modules, statically
  builds `can-utils` (cloned separately into `~/can-utils`, not vendored
  here), and repacks the initramfs.

## Usage

```bash
./build.sh   # copy CAN modules, build can-utils, refresh the initramfs
./run.sh     # boot QEMU (Ctrl-A, X to exit)
```

Inside QEMU:

```sh
insmod /root/can-dev.ko
insmod /root/vcan.ko
ip link add dev vcan0 type vcan
ip link set up vcan0
/root/candump vcan0 &
/root/cansend vcan0 123#DEADBEEF
```

## Status

- **Week 1, day 1** — virtual CAN bus (vcan) working end-to-end: kernel CAN
  core/CAN_DEV/CAN_VCAN enabled, can-utils cross-compiled statically, frames
  sent and received via candump/cansend, verified in QEMU.

- **Week 1, day 2** — custom SocketCAN reader (`can_reader.c`) in C, verified
  against `cansend`.
- **Week 1, day 3** — custom SocketCAN sender (`can_sender.c`) in C. Full
  send/receive loop verified using only custom code, no `can-utils`.

- **Week 2, day 1** — Modbus TCP slave (`modbus_slave.c`, built on libmodbus,
  cross-compiled statically) running in QEMU. Reached from the host through
  QEMU port forwarding (`run-modbus.sh`: host 5020 -> guest 502), verified
  with `mbpoll` (read and write of a holding register).
