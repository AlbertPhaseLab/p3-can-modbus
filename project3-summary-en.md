# Project 3 — CAN/Modbus gateway on embedded Linux (summary)

**Repository:** https://github.com/AlbertPhaseLab/p3-can-modbus

## What it is

A Linux gateway that bridges a CAN bus and Modbus TCP, built and tested entirely in simulation (QEMU arm64 on WSL2). A simulated MCU emits telemetry on CAN; the gateway exposes it as Modbus holding registers; a Modbus client writes a command register and the gateway sends it back as a CAN frame.

```
simulated MCU --CAN 0x100--> gateway (poll loop) <--Modbus TCP-- SCADA client
simulated MCU <--CAN 0x200-- (reg 8 write)
```

## What I built

| Area | Work |
|---|---|
| Kernel | Enabled SocketCAN (`CAN`, `CAN_RAW`, `CAN_DEV`, `CAN_VCAN`) in the kernel tree from project 1; resolved the module dependency chain (`can-dev` before `vcan`) |
| CAN tooling | Cross-compiled `can-utils` statically; wrote a SocketCAN reader and sender in C (`socket`, `ioctl(SIOCGIFINDEX)`, `bind`, `read`/`write` on `struct can_frame`) |
| Modbus | Cross-compiled `libmodbus` statically for arm64; wrote a Modbus TCP slave and master in C |
| Gateway | Single-threaded `poll()` loop serving a CAN socket and the Modbus slave: telemetry (`0x100`) to registers 0-3 in big-endian, writes to register 8 turned into command frames (`0x200`); the request is inspected before replying so repeated writes of the same value are not lost |
| Build system | `build.sh`/`env.sh`/`run.sh` pattern, QEMU user networking with host port forwarding, a per-boot setup script injected into the initramfs |
| Concurrency | A host-side demo showing that two threads without a mutex read half-updated register sets about 6-7 % of the time, versus zero with a mutex, which justifies the single-threaded design |

## Results

- Telemetry from the simulated MCU is readable from the host with a stock Modbus client, and each read matches the last frame logged by the gateway.
- One Modbus write produces exactly one CAN command frame.
- The gateway keeps receiving CAN while a Modbus client is connected.

## Problems found and documented

- **Late command execution:** with one client connected, a second client times out, but its queued request still runs once the gateway is free. Documented as a limitation; the fix is multi-client `poll()` with timeouts.
- **Frozen telemetry:** when the CAN source stops, registers keep their last value and clients cannot detect it; a heartbeat/age register would fix it.
- **Debugging patterns:** interleaved output on a shared serial console looking like "not found" errors, an unset environment variable making `cd` a silent no-op, interfaces that come up down in a minimal rootfs, and a stray character in a host address producing a misleading "connection refused".

## Skills demonstrated

SocketCAN and Linux network APIs in C, `poll()`-based event loops, Modbus TCP, cross-compilation of autotools projects, static linking for minimal rootfs images, QEMU networking, kernel configuration and module dependencies, and design trade-offs between threads and event loops.

## Link with the other projects

- **Project 1** (I2C kernel driver): provides the kernel tree and the rootfs/QEMU workflow.
- **Project 2** (Yocto layer): the natural next step is packaging the gateway, libmodbus and the CAN kernel fragment as Yocto recipes so one image boots with the gateway running.

## Next steps

Port to real hardware (a board with a CAN controller and a real MCU node on the bus), add multi-client support and a heartbeat register, and start the gateway from systemd.
