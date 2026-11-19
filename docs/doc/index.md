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
rho flash -target pico2w -project . build/output/hello_world.elf
```

The platform converts the ELF to UF2 and writes it to the bootrom's drive, or
loads it over PICOBOOT through `picotool` when that is available. A board that
is already running a program is asked to reboot into BOOTSEL by `picotool -f`.
A Pico Shared to a Windows host is attached to WSL first, and re-attached after
the reboot, because the USB device comes back on the other side of the bridge.
Passwordless `sudo` is used only when it is already available; otherwise the
drive is the fallback.

### Flashing as a user

picotool opens the bootrom's USB device, which belongs to root until a udev rule
says otherwise. The platform carries the rule (`udev/60-robohub-pico.rules`,
for every Raspberry Pi device, vendor `2e8a`, to the `plugdev` group) and
declares it in `system_deps`, so `rho doctor` says when it is missing. Install
it once, then unplug and plug the board in again (on WSL, attach it again):

```sh
rho keeper install --path <robohub-pico> robohub-pico 60-robohub-pico.rules
```

Without it the flash says that picotool found the board but may not open it,
names that command, and falls back to the bootrom's drive — slower, and on WSL
done through the Windows host.

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

## Pins

The Pico 2W's defaults, from `boards/pico2w/target.json` (the module.yml and
the board support package say the same):

| Function | GPIO |
|---|---|
| `uart0` (console) | TX GP0, RX GP1 |
| `uart1` | TX GP4, RX GP5 |
| `i2c0` | SDA GP8, SCL GP9 |
| `i2c1` | SDA GP6, SCL GP7 |
| `spi0` | TX GP19, RX GP20, SCK GP18, CSn GP17 |
| `onewire0` | GP28 |
| `dht0` | GP22 |
| `adc0` | GP26 |
| `pwm0` | GP21, GP10, GP11 |
| user LED | on the CYW43, WL_GPIO 0 — not an RP2350 GPIO |

No GPIO carries two functions, so every bus can be used at once, and none is
GP23, GP24, GP25 or GP29, which the board wires to the CYW43 (the SDK's
`boards/pico2_w.h`). `tests/board_pins_test.py` checks that, that each pin is
one the RP2350 can route that function to (the SDK's `io_bank0.h`), and that
the three files agree. The `user_led` GPIO instance is a plain RP2350 GPIO: it
does not light the LED, which needs the CYW43 driver (`cyw43_arch_gpio_put`),
linked only with `RH_NET`.

## The device keystore

The rotated device key (`robohub/keystore.h`) lives in the last two 4 KiB
sectors of flash — on a Pico 2W's 4 MB part, `0x103FE000`-`0x103FFFFF` in the
XIP window — slot A then slot B, with the A/B records, CRC32 and sequence
numbers of `robohub/keystore_core.h`. `src/platform/pico_keystore.c` erases and
programs a sector through the SDK (`hardware_flash`) inside
`flash_safe_execute` (`pico_flash`), which disables interrupts and locks the
other core out; if core 1 runs without having called
`flash_safe_execute_core_init` (and no SDK-aware RTOS is in charge), the store
refuses rather than crash.

The SDK's linker script is left as it is. The platform adds an implicit script,
`cmake/pico_keystore_reserve.ld`, that fails the link when the image's end
reaches the keystore: a program that grows into it does not link, instead of
having its tail erased by a key rotation. The flash size is the board's
(`PICO_FLASH_SIZE_BYTES`), and the source refuses to compile if the two
disagree.

The RP2350's OTP is the better home for a key (it can be locked against
reading), and is planned. It is one-time programmable, so every rotation would
use up rows; until a scheme for that is settled the key stays in flash, which
SWD can read unless debug is disabled.

## Production

`CONFIG_RH_PRODUCTION=y` (`production/production.conf`) builds a signed image
for RP2350 secure boot. Set `RH_SECURE_BOOT_SIGNING_KEY` to a secp256k1 PEM key
(`openssl ecparam -name secp256k1 -genkey -out key.pem`); the build seals the
ELF with picotool and writes the OTP rows that enable secure boot for that key
to `build/output/<program>.otp.json`. Building burns nothing. Signing needs a
picotool built with mbedtls (the SDK's `lib/mbedtls` submodule); one built
without it stops the build at `picotool seal`.

```sh
export RH_PICO_OTP_JSON=build/output/app.otp.json
rho flash -target pico2w build/output/app.elf --production --dry-run
rho flash -target pico2w build/output/app.elf --production
```

`production/rp2350.json` is the plan: with the board in BOOTSEL mode it loads
the OTP file (`picotool otp load`, **irreversible**: BOOTKEY0, BOOT_FLAGS1
KEY_VALID, CRIT1 SECURE_BOOT_ENABLE) and then writes the signed image. The OTP
file's contents are printed in full, and nothing runs until the board name is
typed. Disabling debug (CRIT1 DEBUG_DISABLE) is not part of the plan; it is
irreversible on its own and is left to a deliberate, separate decision.
