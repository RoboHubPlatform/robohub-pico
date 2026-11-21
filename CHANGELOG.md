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

## [0.4.0] - 2026-11-20

The health contracts on the RP2350 (PICO-0010, robohub-common 1.3.0): the hardware watchdog (up to 16 777 ms, stoppable, no early warning), the reset reason from the SDK's watchdog-reboot distinction and POWMAN's CHIP_RESET flags (power-on, brown-out, RUN pin), 32 bytes of uninitialised RAM for the fault record and boot count, and a HardFault handler that records the faulting PC and reboots — so the next boot reports a fault. `hardware_watchdog` is linked; `rh_health_platform` is named on the link so the strong functions always win over robohub-common's weak ones.

## [0.4.1] - 2026-11-20

`examples/c/health_check`, the health contracts checked on the board (PICO-0010): one boot in which a task hangs and the core's watchdog (`CONFIG_RH_CORE_WATCHDOG_MS=3000`) resets the board, one in which a task faults and the HardFault handler records it, and one that reports the record and runs with the watchdog fed. On a Pico 2 W it ended at boot 3 with reason `fault`, a hard fault in task 1 at the faulting store — both resets kept the retained record.

## [0.5.0] - 2026-11-20

Updates over the link on the Pico 2 W (PICO-0011, PICO-0012, robohub ADR-0023).

- **The update slot** (`src/platform/pico_update.c`): an image is written to the A/B partition not booted, a page at a time; booted once by a flash update boot to that partition's start; on trial while the boot ROM says buy-pending; confirmed with `explicit_buy()`; given up with a normal reboot, which boots the untried image. The update's id and versions, its outcome (kept or rolled back) and the board's security version live in the update state sector before the keystore, so the old image reports a rollback after any reset. The partitions' places are read from the boot ROM's table; a board without one is not updatable.
- **The flash layout** (`partitions/ab.json`): the table, partitions A and B of 2036 KiB each, a spare sector, the update state sector, the keystore. `tools/flash_ab.sh` writes the table and the program into partition A; the board manifest names it as `board.flash.ab_command` for Mesh. An image must fit a partition (`cmake/pico_partition_reserve.ld`).
- **Two kinds of image**: `CONFIG_RH_UPDATE_IMAGE` builds the program flagged try-before-you-buy (`PICO_CRT0_IMAGE_TYPE_TBYB`); every build also writes the raw `.bin` a partition takes.
- **A Wi-Fi join keeps the watchdog fed** while it waits, bounded by its timeout: a join takes longer than the core's watchdog period or the boot ROM's 16.7 s trial window.
- `examples/c/update_check`: the on-target test — version 1 flashed with the table, version 2 confirmed from partition B, version 3 hanging on trial and rolled back.

## [0.5.1] - 2026-11-21

- `robohub_console_write_raw` writes a frame in one `stdio_put_string` and flushes it, rather than one `putchar_raw` per byte, which lost bytes on the USB link once several frames went out together (found in robohub-drivers 0.5.7). The node template's console link goes through it (robohub ADR-0025).
- The update check also cuts a transfer halfway and resumes it from the chunk the board names, and offers a downgrade, which the board refuses (PICO-0013).
- The health check also holds a key in the keystore's flash across each reset (PICO-0014). `health_check.py` builds it, flashes it and reads the console. The check's keys have their own scope and epoch 0, so no session uses them, but they replace a key the board was rotated to.
- `tools/flash_ab.sh` waits up to 90 s, not 40, for the board to come back in BOOTSEL: under WSL, usbipd's attach loop has taken longer than that.

## [0.6.0] - 2026-11-21

The I2C buses are registered (PICO-0015, robohub ADR-0026). With the I2C HAL chosen, the board registers "i2c0" on GP8/GP9 and "i2c1" on GP6/GP7. Their pins are muxed and pulled up when a driver brings a bus up, and `hardware_i2c` is linked. Every transfer is bounded by a timeout, so a bus held low is `ROBOHUB_ERR_TIMEOUT` rather than a stopped loop. No I2C bus was registered before, so no I2C driver (the BMP180's included) found one on the board.

## [0.6.1] - 2026-11-21

The I2C functions take the HAL's own types (`rh_u8`, `rh_size`, `rh_u32`), which the compiler warned about. They are the same width on the RP2350, so nothing ran differently. Hardware I2C on GP8/GP9 is checked on a Pico 2 W: a GY-68 answers at 0x77 with its chip id, 0x55.
