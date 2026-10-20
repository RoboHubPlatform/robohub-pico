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

## Dependencies

- [robohub-common](https://github.com/RoboHubPlatform/robohub-common) — HAL interfaces
- [Pico SDK](https://github.com/raspberrypi/pico-sdk) — Raspberry Pi's official SDK
