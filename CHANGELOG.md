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
