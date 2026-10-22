# RoboHub Pico Platform

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
rho flash -target pico2w build/output/main
```

## The examples

`examples/c/hello_world` is the smallest program that runs on real hardware.

`examples/rho/hello_world` is that program in Rho, written against the
contracts and nothing else.
