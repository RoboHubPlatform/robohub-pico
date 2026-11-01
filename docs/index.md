# RoboHub Pico Platform

**Version:** 0.2.0

The RP2040 and RP2350 platform for the RoboHub ecosystem: the interfaces
declared by `robohub-common`, implemented on Raspberry Pi Pico boards, with
board support packages for the Pico boards.

## The platform

A Rho program on this platform runs on a Pico. The platform provides the
interfaces, the HAL, and the board support package that registers the board's
peripherals.

## Building

```sh
cd examples/c/hello_world
rho build -target pico2w .
```

## Flashing

```sh
rho flash -target pico2w -project . build/output/hello_world.elf
```

The platform converts the ELF to UF2 and writes it to the bootrom's drive, or
loads it over PICOBOOT through `picotool` when that is available. A board that
is already running a program is asked to reboot into BOOTSEL by `picotool -f`.
A Pico Shared to a Windows host is attached to WSL first, and re-attached after
the reboot, because the USB device comes back on the other side of the bridge.
Passwordless `sudo` is used only when it is already available; otherwise the
drive is the fallback.

## The examples

`examples/c/hello_world` is the smallest program that runs on real hardware.

`examples/rho/hello_world` is that program in Rho, written against the
contracts and nothing else.

`examples/c/freertos_hello_world` is that program with its work in a FreeRTOS
task: it links `robohub-freertos` and uses the RoboHub OSAL, so the same source
runs with or without the kernel beneath it. The board's USB console is pumped by
a task above the ordinary task priorities, so a console write cannot starve the
USB stack it waits for.

The board manifest declares the roles and transports a deployment planner may
offer (`gateway`, `relay`, `node`, `edge`, `wireless`, `links` and `buses`) and
the `deployment` block names how a gateway is built and reached. Adding a board
does not require a keeper rebuild.
