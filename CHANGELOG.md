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
