# robohub-pico

RoboHub platform for Raspberry Pi Pico boards (RP2040 and RP2350).

## Boards

- **Pico 2W** — RP2350, Cortex-M33, 4MB flash, 520KB SRAM, WiFi/BT

## Interfaces

The platform implements the contracts of `robohub-common`: GPIO, I2C, SPI, UART,
timer, RS485, OneWire, DHT, and — for the parts that are neither on a bus nor
digital — an **ADC** and a **PWM**.

| Contract | On the Pico |
|---|---|
| GPIO | the SDK's pins, as `gpio0` and the LED as `user_led` |
| I2C | `i2c0` on GP16/GP17, `i2c1` on GP18/GP19 |
| SPI | `spi0` on GP19/GP20/GP18/GP17 |
| UART | `uart0` on the console pins, `uart1` on GP4/GP5 |
| timer | `system_timer`, the SDK's timer |
| RS485 | a UART with a transceiver's direction pin |
| OneWire | `onewire0` on GP28 |
| DHT | `dht0` on GP22 |
| ADC | `adc0`, the converter's input on GP26 (the part has four) |
| PWM | `pwm0` on GP21, GP23, GP24 — three channels, enough for an RGB LED |

An interface is compiled in and registered only when a program selects its HAL, so
a program that reads no analog part pays for none of it.

### The ADC

The converter is the RP2350's: four inputs and twelve bits. A reading is a level
from 0 to 1 of full scale, so a driver compares it and never asks how many bits
there were. The pins are the board's to say, and the board says one.

### The PWM

The counter runs at the system clock over a fixed divider, and the wrap is what
sets the frequency — which keeps a frequency as exact as the divider allows and
leaves a duty a plain fraction of the wrap. Two channels of one slice share that
slice's counter, so they share its frequency; a peripheral has one frequency, and
a board that wants two wants two peripherals.

## Deployment

The Pico 2W's board manifest declares what the board can be and how a
deployment reaches it:

```yaml
capabilities:
  gateway: true
  relay: true
  node: true
  edge: true
  host: false
  wireless: true
  links: "uart console usb wifi socket"
  buses: "uart usb i2c spi onewire gpio adc pwm"
  deploy: "flash"

deployment:
  method: "flash"
  arch: "arm"
  kconfig: "CONFIG_RH_BOARD_PICO_2W=y"
```

The keeper's Mesh planner reads these to decide which roles and transports the
board can take.

## Flashing

`rho flash` uses `tools/elf2uf2/flash`. It:

- attaches a Pico that is Shared to the Windows host to WSL, so PICOBOOT is
  visible,
- uses `picotool -f` to reboot a running board into BOOTSEL,
- re-attaches the board after that reboot, because the USB device comes back on
  the other side of the bridge,
- uses passwordless `sudo` only when it is already available,
- falls back to the bootrom's drive.

A board already in BOOTSEL is used as it is. `RH_PICOTOOL` names a picotool the
machine keeps elsewhere.

## Dependencies

- [robohub-common](https://github.com/RoboHubPlatform/robohub-common) — HAL interfaces
- [Pico SDK](https://github.com/raspberrypi/pico-sdk) — Raspberry Pi's official SDK

## Release history

[CHANGELOG.md](CHANGELOG.md) — the changes in 0.2.0 and earlier releases.
