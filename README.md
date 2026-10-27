# p3-can-modbus — Embedded Linux Portfolio, project 3/3

Linux + MCU-style gateway: SocketCAN (virtual CAN bus) on one side, Modbus TCP on the other, fully simulated in QEMU (`-M virt`, arm64) from WSL2. No real hardware is required. Reuses the kernel tree from [p1-i2c](https://github.com/AlbertPhaseLab/p1-i2c), extended with `CONFIG_CAN`, `CONFIG_CAN_VCAN` and `CONFIG_CAN_RAW`.

The end goal is a gateway process that translates between both worlds: CAN telemetry frames end up in Modbus holding registers, and Modbus writes are forwarded to the CAN bus as command frames. All code is plain C and portable: it only depends on SocketCAN and TCP/IP, so moving to a real board (Raspberry Pi with a CAN HAT, STM32MP1/MP2) mainly means swapping `vcan0` for a real `can0`.

## Repository layout

| Path | Purpose |
|---|---|
| `env.sh` | Shared environment: cross-compiler, kernel tree (`KDIR`), rootfs (`ROOTFS`), initramfs (`INITRAMFS`), can-utils checkout (`CANUTILS_DIR`) and libmodbus install prefix (`MODBUS_PREFIX`) |
| `build.sh` | Copies the CAN kernel modules, builds can-utils and all custom programs (statically), copies `rootfs-extra/` content, and repacks the initramfs |
| `run.sh` | Boots QEMU without networking (CAN-only work) |
| `run-modbus.sh` | Boots QEMU with a virtio NIC and port forwarding (host `5020` -> guest `502`) so the host can reach the Modbus slave |
| `src/can_reader.c` | SocketCAN receiver: reads frames from a CAN interface and prints them |
| `src/can_sender.c` | SocketCAN sender: builds and transmits a frame from the command line |
| `src/modbus_slave.c` | Modbus TCP slave (the simulated MCU): 16 holding registers, register 0 preloaded with `1234` |
| `src/modbus_master.c` | Modbus TCP master (client): reads and writes holding registers |
| `src/gateway.c` | Gateway: single-threaded `poll()` loop that copies CAN telemetry (ID `0x100`) into Modbus holding registers 0-3 |
| `rootfs-extra/p3-setup.sh` | Per-boot setup script run inside the VM (loopback, `eth0`, CAN modules, `vcan0`) |

## Prerequisites

- WSL2 with Ubuntu 24.04, `aarch64-linux-gnu-gcc`, QEMU (`qemu-system-aarch64`)
- Kernel tree from project 1 in `~/linux`, with the CAN options above enabled and rebuilt (`make Image modules`)
- [can-utils](https://github.com/linux-can/can-utils) cloned into `~/can-utils` (not vendored in this repo)
- [libmodbus](https://github.com/stephane/libmodbus) cross-compiled for arm64 into `~/libmodbus-arm64`:

```bash
cd ~/libmodbus
./autogen.sh
CC=aarch64-linux-gnu-gcc ./configure --host=aarch64-linux-gnu \
  --enable-static --disable-shared --prefix=$HOME/libmodbus-arm64
make -j"$(nproc)" && make install
```

- Optional, host side only: `sudo apt install mbpoll` to query the slave from Ubuntu

The `getaddrinfo` linker warning that appears when linking against a static libmodbus is expected and harmless: the programs only use literal IP addresses, never hostname resolution.

## Usage

```bash
./build.sh          # rebuild everything and refresh the initramfs
./run.sh            # boot without networking (Ctrl-A, X to exit)
./run-modbus.sh     # boot with host:5020 -> guest:502 forwarding
```

`run.sh` and `run-modbus.sh` only boot the existing kernel image and initramfs; they do not build anything. Run `./build.sh` after changing any `.c` file, anything in `rootfs-extra/`, or a kernel module (the kernel itself is not rebuilt by `build.sh`; rebuild it in `~/linux` first if its configuration changed).

### Per-boot setup inside the VM

Everything configured inside QEMU lives in RAM and is lost on exit. Network interfaces (`lo`, `eth0`) also come up *down* in this minimal rootfs. `rootfs-extra/p3-setup.sh` does it all in one go:

```sh
/root/p3-setup.sh
```

It brings up the loopback, brings up `eth0` with `10.0.2.15/24` if a NIC exists (it does not under `run.sh`), loads `can-dev.ko` and `vcan.ko` in dependency order, and creates and raises `vcan0`. Run it **inside QEMU only**, never on the host.

### CAN

```sh
/root/candump vcan0 &                      # can-utils reference tools
/root/cansend vcan0 123#DEADBEEF

/root/can_reader vcan0 &                   # custom receiver
/root/can_sender vcan0 123 DE AD BE EF     # custom sender: interface, hex ID, hex data bytes
```

`cangen` from can-utils can act as a periodic telemetry generator, e.g. `cangen vcan0 -I 100 -L 8 -g 500` (ID `0x100`, 8 bytes, every 500 ms).

### Modbus TCP

Inside the VM (loopback, after `p3-setup.sh`):

```sh
/root/modbus_slave &
/root/modbus_master 127.0.0.1 502 r 0 1     # read 1 register from address 0
/root/modbus_master 127.0.0.1 502 w 0 4321  # write 4321 to address 0
```

From the host, through the QEMU port forward (boot with `./run-modbus.sh`, run `p3-setup.sh` and start the slave first):

```bash
mbpoll -m tcp -p 5020 -a 1 -r 1 -c 1 -1 127.0.0.1         # read
mbpoll -m tcp -p 5020 -a 1 -r 1 -1 127.0.0.1 -- 4321      # write
```

Addressing note: libmodbus addresses are 0-based while `mbpoll -r` is 1-based, so `mbpoll -r 1` is address `0` in `modbus_master`.

The slave keeps its register table in process memory, so values written by one client persist across connections and are visible to every other client.

## Notes

- **The rootfs is shared with project 1.** Both projects use the same `~/rootfs` and `~/initramfs.cpio.gz`, so their `build.sh` scripts write into one image. Run the `build.sh` of the project whose `.c` files you changed. Giving each project its own `ROOTFS`/`INITRAMFS` in `env.sh` would isolate them.
- **Where things run.** Compiling, `build.sh`, `mbpoll` and Git run on the host. Only what is copied into `~/rootfs/root/` exists inside the VM (there is no `mbpoll` in the guest; `modbus_master` plays that role).
- **Empty `$ROOTFS`.** In a fresh shell, source `env.sh` before running manual commands that use its variables (`cd ""` silently does nothing in bash). An interactive alias such as `alias p3env='source ~/p3-can-modbus/env.sh'` helps; aliases do not work inside scripts, which source `env.sh` themselves.
- **Garbled serial output.** Launching a process with `&` on the QEMU serial console can interleave its output with the shell's, producing misleading "not found" lines. Wait for the process's start-up message before typing the next command, or run commands in the foreground.

## Status

- **Week 1, day 1** — virtual CAN bus (vcan) working end-to-end: kernel CAN core/CAN_DEV/CAN_VCAN enabled, can-utils cross-compiled statically, frames sent and received via candump/cansend, verified in QEMU.
- **Week 1, day 2** — custom SocketCAN reader (`can_reader.c`) in C, verified against `cansend`.
- **Week 1, day 3** — custom SocketCAN sender (`can_sender.c`) in C. Full send/receive loop verified using only custom code, no `can-utils`.
- **Week 2, day 1** — Modbus TCP slave (`modbus_slave.c`, built on libmodbus, cross-compiled statically) running in QEMU. Reached from the host through QEMU port forwarding (`run-modbus.sh`: host 5020 -> guest 502), verified with `mbpoll` (read and write of a holding register).
- **Week 2, day 2** — Modbus TCP master (`modbus_master.c`) in C, verified against the custom slave over loopback inside QEMU (read, write, read-back, state persisting across connections). `build.sh` reorganised so every custom program is compiled before the initramfs is repacked. `rootfs-extra/p3-setup.sh` added to automate per-boot setup.
- **Week 3, day 1** — `gateway.c`: single-threaded `poll()` loop serving a CAN socket and the Modbus slave at once. CAN telemetry (ID `0x100`) is copied into holding registers 0-3 (big-endian); verified in QEMU with `cansend`/`cangen` and `modbus_master`.

## Roadmap

- **Week 3** — the gateway itself: a single-threaded `poll()` loop serving a CAN socket and the Modbus slave at once. Planned mapping: CAN ID `0x100` (telemetry, 8 bytes) -> holding registers 0-3 (big-endian, matching `candump` output); a write to register 8 -> CAN frame with ID `0x200` (command).
- Optional final step: port to real hardware (`can0` with an explicit bitrate, vendor kernel via Yocto, services started by systemd).
