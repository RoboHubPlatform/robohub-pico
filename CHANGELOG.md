# Changelog

All notable changes to this module are recorded here, one section per release.

## [0.2.0] - 2026-11-01

The flash entry attaches a Pico Shared to a Windows host to WSL, reboots a
running board into BOOTSEL, re-attaches it after that reboot, uses passwordless
`sudo` only when available and falls back to the bootrom's drive. The board
manifest declares the roles, links, buses and flash deployment a Mesh planner
reads. The docs cover the FreeRTOS hello-world example and the USB-CDC pump.

## [0.2.1] - 2026-11-13

A Pico in BOOTSEL that Windows has not shared with WSL yet is shared by the
flash entry, which asks Windows for an administrator's consent once, and then
attached; before, only a board already shared was attached, and the flash found
no device.

## [0.2.2] - 2026-11-13

The flash entry finds powershell.exe where Windows keeps it when a service's PATH lost the Windows directories, so on WSL it can still attach a board, load it over PICOBOOT or write the UF2 through Windows.

## [0.2.3] - 2026-11-13

The 1-Wire bus runs each bit slot and the presence sample with interrupts off: a USB or Wi-Fi interrupt inside a slot stretched it, which read as a failed CRC or a missing presence pulse from the DS18B20.

## [0.3.0] - 2026-11-14

The board key is kept in the last two flash sectors, reserved at link time. A production preset signs the image and lists the OTP steps, which `rho flash --production` runs only when confirmed. Host tests configure without the SDK.

## [0.3.1] - 2026-11-14

The platform carries the udev rule a user needs to flash a board in BOOTSEL (`udev/60-robohub-pico.rules`, declared in `system_deps`, reported by `rho doctor`, installed by `rho keeper install`). When picotool finds the board but may not open it, the flash says so and names the command that installs the rule, before it falls back to the bootrom's drive.

## [0.3.2] - 2026-11-15

`elf2uf2` 0.2.0 is written with `Slice[u8]`, which rho 0.15.0 requires: it reads the ELF as a `Slice[u8]`, builds the UF2 in one buffer sized for 16 MB of flash and writes it once, instead of growing a byte array block by block. Its output is byte-for-byte what it was, and what picotool writes, for every firmware tested. An image spanning more than 16 MB of flash, or with more than 32 loadable segments, is refused with a message.

## [0.3.3] - 2026-11-19

Pico 2W pin defaults no longer collide. I2C1 sat on SPI0's SCK and TX (GP18/19) and I2C0's SCL on SPI0's CSn (GP17), so the defaults could not be used together; I2C0 is now on GP8/GP9 and I2C1 on GP6/GP7, SPI0 unchanged. PWM channels 1 and 2 were on GP23 and GP24, which a Pico 2W wires to the CYW43 (WL_REG_ON and the data line, per the SDK's boards/pico2_w.h): selecting the PWM HAL took the wireless chip's control lines. They are now GP10/GP11. `user_led` was declared on GP15, which has no LED; it is now declared as the CYW43's WL_GPIO 0 (CYW43_WL_GPIO_LED_PIN), as the SDK does — the `user_led` GPIO instance still drives a plain RP2350 GPIO and lights nothing. The board manifest listed `pwm0` three times and omitted the I2C, SPI and DHT pins; it now gives the same pins as target.json. New host suite `board-pins` (tests/tests.yml, the platform's first) checks that no GPIO is claimed twice or on the CYW43's lines, that each peripheral pin is one IO_BANK0 can route that function to, and that target.json, module.yml, bsp.h and bsp.c agree. Under FreeRTOS the board defines `robohub_freertos_console_drain` (robohub-freertos 0.1.2): after a kernel panic line it pumps the USB CDC console until its FIFO is empty (at most 250 ms) or waits out the UART FIFO, so the line reaches the host before the board halts (PICO-HAL-0006). Removed `examples/c/hello_world/--family-id`, a 10 KB UF2 image committed by accident: an early elf2uf2 took `--family-id` as its output path.

## [0.3.4] - 2026-11-20

The FreeRTOS example names robohub-freertos at its new path in the robohub-node collection (`kernels/freertos/`; robohub ADR-0020); CI checks out robohub-node instead of robohub-freertos.
