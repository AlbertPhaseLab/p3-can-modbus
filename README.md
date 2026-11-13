# p3-can-modbus — Embedded Linux Portfolio, project 3/3

A Linux gateway between a **CAN bus** (field side, MCU-style telemetry and commands) and **Modbus TCP** (supervision side, SCADA-style clients), fully simulated in QEMU (`-M virt`, arm64) from WSL2. No real hardware is required. It reuses the kernel tree from [p1-i2c](https://github.com/AlbertPhaseLab/p1-i2c), extended with `CONFIG_CAN`, `CONFIG_CAN_VCAN` and `CONFIG_CAN_RAW`.

The gateway is a single-threaded C program built around `poll()`:

```
 "MCU"                          Linux gateway                       "SCADA"
 cangen / can_sender  --CAN-->  gateway (poll loop)  <--Modbus TCP-- mbpoll / modbus_master
   (ID 0x100 telemetry)         holding registers 0-15              (read regs 0-3,
   (ID 0x200 command)  <--CAN-- write to reg 8 -> frame 0x200        write reg 8)
```

| Direction | Source | Destination |
|---|---|---|
| CAN -> Modbus (telemetry) | CAN frame ID `0x100`, up to 8 bytes | Holding registers 0-3, each byte pair as one big-endian 16-bit register |
| Modbus -> CAN (command) | Write to holding register 8 (function `0x06` or `0x10`) | CAN frame ID `0x200`, 2 bytes, big-endian |

All code is plain C and only depends on SocketCAN and TCP/IP, so moving to a real board mainly means replacing `vcan0` with a real `can0` (with an explicit bitrate).

## Repository layout

| Path | Purpose |
|---|---|
| `env.sh` | Shared environment: cross-compiler, kernel tree (`KDIR`), rootfs (`ROOTFS`), initramfs (`INITRAMFS`), can-utils checkout (`CANUTILS_DIR`) and libmodbus prefix (`MODBUS_PREFIX`) |
| `build.sh` | Copies the CAN kernel modules, builds can-utils and all custom programs statically, copies `rootfs-extra/`, and repacks the initramfs |
| `run.sh` | Boots QEMU without networking (CAN-only work) |
| `run-modbus.sh` | Boots QEMU with a virtio NIC and port forwarding (host `5020` -> guest `502`) |
| `src/gateway.c` | The gateway: `poll()` loop over a CAN socket and the Modbus slave; telemetry to registers 0-3, writes to register 8 to CAN command frames |
| `src/modbus_slave.c` | Standalone Modbus TCP slave (16 holding registers) |
| `src/modbus_master.c` | Modbus TCP master (client) for reading and writing registers |
| `src/can_reader.c` | SocketCAN receiver |
| `src/can_sender.c` | SocketCAN sender |
| `rootfs-extra/p3-setup.sh` | Per-boot setup inside the VM (loopback, `eth0`, CAN modules, `vcan0`) |
| `demos/race_demo.c` | Host-side demo of torn reads between two threads, with and without a mutex |

## Prerequisites

- WSL2 with Ubuntu 24.04, `aarch64-linux-gnu-gcc`, QEMU (`qemu-system-aarch64`)
- Kernel tree from project 1 in `~/linux`, with the CAN options above enabled and rebuilt (`make Image modules`)
- [can-utils](https://github.com/linux-can/can-utils) cloned into `~/can-utils` (not vendored here)
- [libmodbus](https://github.com/stephane/libmodbus) cross-compiled for arm64:

```bash
cd ~/libmodbus
./autogen.sh
CC=aarch64-linux-gnu-gcc ./configure --host=aarch64-linux-gnu \
  --enable-static --disable-shared --prefix=$HOME/libmodbus-arm64
make -j"$(nproc)" && make install
```

- Optional, host side: `sudo apt install mbpoll`

The `getaddrinfo` linker warning with a static libmodbus is harmless: only literal IP addresses are used, never hostname resolution.

## Usage

```bash
./build.sh          # rebuild everything and refresh the initramfs
./run-modbus.sh     # boot with host:5020 -> guest:502 forwarding (use ./run.sh for CAN-only work)
```

`run.sh` and `run-modbus.sh` only boot the existing kernel image and initramfs. Run `./build.sh` after changing any `.c` file, anything in `rootfs-extra/`, or a kernel module (the kernel itself is not rebuilt by `build.sh`).

Everything configured inside QEMU lives in RAM, and network interfaces come up *down* in this minimal rootfs, so after each boot, **inside QEMU**:

```sh
/root/p3-setup.sh                      # lo, eth0 (if present), can-dev/vcan modules, vcan0
/root/gateway vcan0 &                  # wait for "Gateway listo" before typing more
```

### Try it

Simulated MCU telemetry and a command, inside the VM:

```sh
/root/cangen vcan0 -I 100 -L 8 -g 500 -n 40 &     # 40 telemetry frames, ID 0x100
/root/candump vcan0 &
/root/modbus_master 127.0.0.1 502 r 0 4           # read registers 0-3 (latest telemetry)
/root/modbus_master 127.0.0.1 502 w 8 4660        # write 0x1234 to reg 8 -> CAN 0x200 [12 34]
```

From the host, through the QEMU port forward:

```bash
mbpoll -m tcp -p 5020 -a 1 -r 1 -c 4 -t 4:hex -1 127.0.0.1     # telemetry, in hex
mbpoll -m tcp -p 5020 -a 1 -r 9 -1 127.0.0.1 -- 99             # command (register 8)
```

Addressing note: libmodbus is 0-based and `mbpoll -r` is 1-based, so register 8 is `mbpoll -r 9`.

Individual tools: `can_sender vcan0 123 DE AD BE EF`, `can_reader vcan0`, and the standalone `modbus_slave` (do not run it together with the gateway: both bind port 502).

### Concurrency demo (host only)

```bash
cd demos && gcc -O2 -Wall -Wextra -pthread race_demo.c -o race_demo
./race_demo           # without mutex: ~6-7 % of reads see a half-updated "frame"
./race_demo mutex     # with mutex: 0
```

This is why the gateway uses one thread: it writes the telemetry registers and answers Modbus reads sequentially in the same loop, so a read never observes a half-updated frame, with no locking.

## Verified behaviour

- Telemetry from `cangen` is visible from the host through `mbpoll`; each read matches the last frame logged by the gateway.
- A Modbus write to register 8 emits exactly one `0x200` frame per write, including repeated writes of the same value (the request is inspected before replying, instead of comparing register values).
- The gateway keeps receiving CAN while a Modbus client is connected.

## Known limitations

- **One Modbus client at a time.** While a client is connected, others are queued by the kernel; their requests are executed once the gateway is free, so a command can run *after* its sender already timed out. A production gateway should poll multiple clients and use timeouts.
- **No staleness indication.** If the CAN source stops, the registers keep their last value; clients cannot tell the "MCU" is gone (a heartbeat/age register would solve it).
- Modbus function `0x17` (read/write multiple registers) is not inspected for writes to register 8.
- No signal handling: the gateway is stopped by killing the process.
- The rootfs and initramfs are shared with project 1: run the `build.sh` of the project whose files you changed.

## Notes

- Where things run: compiling, `build.sh`, `mbpoll` and Git run on the host; only what is copied into `~/rootfs/root/` exists inside the VM (there is no `mbpoll` in the guest).
- In a fresh host shell, source `env.sh` before running manual commands that use its variables (an unset `$ROOTFS` makes `cd "$ROOTFS"` silently do nothing).
- Launching a process with `&` on the QEMU serial console can interleave its output with the shell's; wait for its start-up message before typing the next command.

## Status

- **Week 1** — CAN: virtual CAN bus (`vcan`), can-utils cross-compiled statically, custom SocketCAN reader and sender in C.
- **Week 2** — Modbus TCP: libmodbus cross-compiled statically; slave and master in C; QEMU port forwarding; `p3-setup.sh` for per-boot setup.
- **Week 3** — Gateway: single-threaded `poll()` loop, bidirectional CAN <-> Modbus mapping; integration test with an external Modbus client; mutex demo; limitations documented.

## Next steps

Port to real hardware: `can0` with an explicit bitrate on a board with a CAN controller (for example a Raspberry Pi with a CAN HAT, or an STM32MP board), a real MCU node on the bus, and boot-time startup through systemd or a Yocto image (see the companion Yocto layer from project 2).
