---
id: ADR-0001
title: Keep the device key in the last two flash sectors, reserved by a link-time check
status: proposed
date: 2026-11-14
requirements: [PICO-0004, PICO-KEY-0001, PICO-KEY-0002, SYS-0022]
supersedes: []
superseded_by: []
---

# Keep the device key in the last two flash sectors, reserved by a link-time check

## Context

A board's key is rotated over its session without flashing it
(robohub:ADR-0010), so the rotated key must survive a reset and a reflash. The
RP2350 has no on-chip flash and no EEPROM; it has the external flash the image
runs from, and OTP. OTP can be locked against reading, but it is one-time
programmable: every rotation would use up rows. The SDK's linker script places
the image from the start of flash and knows nothing of a keystore.

## Decision

The keystore is the last two 4 KiB sectors of the board's flash, slot A then
slot B, holding robohub-common's A/B records (robohub/keystore_core.h). The
SDK's linker script is left as the vendor wrote it; an implicit linker script
(`cmake/pico_keystore_reserve.ld`) beside it asserts that the image ends below
`XIP_BASE + PICO_FLASH_SIZE_BYTES - 2 * 4096`, so a program that grows into the
keystore fails to link. The source refuses to compile when its flash size is not
the board's. Erase and program go through `flash_safe_execute`. OTP is left for
later.

## Consequences

- A key survives a reset and a reflash: picotool and the UF2 drive write only
  the sectors the image covers.
- The key is readable over SWD unless debug is disabled, which the production
  plan deliberately does not do (PICO-PROD-0003).
- Moving the key into OTP needs a scheme for rows per epoch first; that is a new
  decision superseding this one.
- A board with another flash size gets the right sectors from its board header
  without a change here.
