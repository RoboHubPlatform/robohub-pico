---
id: ADR-0002
title: Flash over PICOBOOT first, and on WSL write through the Windows host
status: proposed
date: 2026-11-13
requirements: [PICO-0003, PICO-FLASH-0002, PICO-FLASH-0003]
supersedes: []
superseded_by: []
---

# Flash over PICOBOOT first, and on WSL write through the Windows host

## Context

An RP2350 is programmed by its bootrom, either over the PICOBOOT USB interface
or by a UF2 file copied to the mass-storage drive it publishes in BOOTSEL. A
board already running a program publishes no drive. The keeper often runs on
WSL, whose kernel has no usb-storage: the board must be handed through usbipd,
and a file written to the drive through another filesystem is cached there and
never reaches the bootrom.

## Decision

The platform carries its own flash entry (`tools/elf2uf2/flash`), which the
board's flash tool resolves to. It converts the ELF to UF2 with the platform's
elf2uf2, then prefers PICOBOOT: `picotool load -f -x`, which reboots a running
board into BOOTSEL. On WSL it first shares and attaches the board through
usbipd (asking Windows for an administrator's consent once), and attaches it
again after the reboot. Only when PICOBOOT fails does it fall back to the
drive; on WSL the file is written by the Windows host, which reads it through
`\\wsl.localhost` and flushes it to the device. Root is used only through
passwordless sudo.

## Consequences

- A board is flashed whether it runs a program or sits in BOOTSEL, on Linux
  and WSL, without a person copying a file.
- picotool as a user needs the udev rule the platform carries
  (`udev/60-robohub-pico.rules`); without it the flash names the fix and falls
  back to the slower drive.
- The WSL path depends on usbipd-win and powershell.exe on the Windows side.
