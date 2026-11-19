#!/usr/bin/env python3
"""Pin assignments in a board spec do not collide, and say the same everywhere.

For every board under boards/ with a target.json, this checks:

- no GPIO is given to two functions: every `pins` entry and every pin a
  `peripherals` entry names is collected, and a GPIO that two different
  functions claim is a failure (an entry may name the same pin as the `pins`
  entry for the same role, e.g. `i2c0_sda` and `peripherals.i2c0.pins.sda`);
- no pin is one the board wires to its wireless chip — the CYW43 pins the Pico
  SDK's board header states (`CYW43_DEFAULT_PIN_*`, `PICO_VSYS_PIN`), read from
  vendor/pico-sdk/src/boards/include/boards/<board header>.h;
- each peripheral pin is one the part can route that function to, read from the
  SDK's IO_BANK0 function-select values (io_bank0.h) — I2C0 SDA only where
  `IO_BANK0_GPIOn_CTRL_FUNCSEL_VALUE_I2C0_SDA` exists, and so on;
- `pins` and `peripherals` agree with each other, module.yml's `board.pins`
  gives the same GPIOs as target.json, and module.yml lists each peripheral
  once and every peripheral target.json declares;
- the board support package compiles the same numbers: bsp.h's RH_*_PIN macros
  and bsp.c's pin arrays match the spec;
- an LED the board has only on the wireless chip is declared as such
  (`cyw43_gpio`, the SDK's CYW43_WL_GPIO_LED_PIN) and not as a GPIO.

Exit 0 when everything holds, 1 with every failure listed.
"""

# @verifies PICO-BOARD-0003

import glob
import json
import os
import re
import sys

try:
    import yaml
except ImportError:  # pragma: no cover - the CI image has it
    print("FAIL: this test reads module.yml with PyYAML (python3-yaml), which is not installed")
    sys.exit(1)

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SDK = os.path.join(ROOT, "vendor", "pico-sdk")
FAILURES = []

# The SDK board header for each board directory, as the board's sdk.cmake names it.
BOARD_HEADERS = {"pico2w": "pico2_w.h"}

# Which IO_BANK0 function a role on a peripheral needs.
FUNCTION = {
    ("uart", "tx"): "UART{n}_TX", ("uart", "rx"): "UART{n}_RX",
    ("i2c", "sda"): "I2C{n}_SDA", ("i2c", "scl"): "I2C{n}_SCL",
    ("spi", "tx"): "SPI{n}_TX", ("spi", "rx"): "SPI{n}_RX",
    ("spi", "sck"): "SPI{n}_SCLK", ("spi", "csn"): "SPI{n}_SS_N",
}


def fail(message):
    FAILURES.append(message)


def read(path):
    with open(path) as handle:
        return handle.read()


def reserved_pins(board):
    """The GPIOs the SDK's board header gives to the wireless chip."""
    header = BOARD_HEADERS.get(board)
    if not header:
        return {}
    path = os.path.join(SDK, "src", "boards", "include", "boards", header)
    if not os.path.exists(path):
        fail(f"{board}: the SDK board header {header} is not in vendor/pico-sdk; cannot check the wireless chip's pins")
        return {}
    pins = {}
    for name, value in re.findall(r"#define\s+(CYW43_DEFAULT_PIN_\w+|PICO_VSYS_PIN)\s+(\d+)u?", read(path)):
        pins.setdefault(int(value), []).append(name)
    return pins


def funcsel():
    """GPIO -> the set of functions IO_BANK0 can route to it."""
    path = os.path.join(SDK, "src", "rp2350", "hardware_regs", "include", "hardware", "regs", "io_bank0.h")
    if not os.path.exists(path):
        fail("io_bank0.h is not in vendor/pico-sdk; cannot check which pins carry which function")
        return {}
    table = {}
    for gpio, function in re.findall(r"IO_BANK0_GPIO(\d+)_CTRL_FUNCSEL_VALUE_(\w+)\b", read(path)):
        table.setdefault(int(gpio), set()).add(function)
    return table


def check_board(board_dir, routes):
    board = os.path.basename(board_dir)
    spec = json.loads(read(os.path.join(board_dir, "target.json")))
    manifest = yaml.safe_load(read(os.path.join(board_dir, "module.yml"))) or {}
    reserved = reserved_pins(board)

    # Every claim on a GPIO: (gpio, function, where).
    claims = []
    for name, pin in (spec.get("pins") or {}).items():
        if "gpio" in pin:
            claims.append((int(pin["gpio"]), name, f"pins.{name}"))
        elif "cyw43_gpio" not in pin:
            fail(f"{board}: pins.{name} names neither a gpio nor a cyw43_gpio")
    for name, peripheral in (spec.get("peripherals") or {}).items():
        for role, value in (peripheral.get("pins") or {}).items():
            function = f"{name}_{role}"
            gpio = int(value)
            claims.append((gpio, function, f"peripherals.{name}.pins.{role}"))
            pin = (spec.get("pins") or {}).get(function)
            # The pins entry for the same role must agree with the peripheral.
            if pin is None:
                # Peripheral roles named by channel (adc channel0 / pwm channel1)
                # are listed in pins under the short form adc0_ch0 / pwm0_ch1.
                short = f"{name}_{role.replace('channel', 'ch')}"
                pin = (spec.get("pins") or {}).get(short)
                function = short
            if pin is None:
                fail(f"{board}: peripherals.{name}.pins.{role} has no pins entry")
            elif int(pin.get("gpio", -1)) != gpio:
                fail(f"{board}: peripherals.{name}.pins.{role} is GP{gpio} but pins.{function} is GP{pin.get('gpio')}")
            # And the part must be able to route the function there.
            kind = re.sub(r"\d+$", "", peripheral.get("instance", name))
            number = re.search(r"(\d+)$", peripheral.get("instance", name) or "0").group(1)
            needed = FUNCTION.get((kind, role))
            if needed and routes:
                needed = needed.format(n=number)
                if needed not in routes.get(gpio, set()):
                    fail(f"{board}: {name}.{role} is on GP{gpio}, which IO_BANK0 cannot route {needed} to")
            if kind == "pwm" and routes and not any(f.startswith("PWM_") for f in routes.get(gpio, set())):
                fail(f"{board}: {name}.{role} is on GP{gpio}, which has no PWM output")

    owners = {}
    for gpio, function, where in claims:
        # Strip the peripheral's role naming so pins.i2c0_sda and
        # peripherals.i2c0.pins.sda count as one function.
        canonical = function.replace("channel", "ch")
        owners.setdefault(gpio, {}).setdefault(canonical, []).append(where)
        if gpio in reserved:
            fail(f"{board}: {where} is GP{gpio}, which the board wires to its wireless chip ({', '.join(reserved[gpio])})")
    for gpio, functions in sorted(owners.items()):
        if len(functions) > 1:
            listed = "; ".join(f"{name} ({', '.join(places)})" for name, places in sorted(functions.items()))
            fail(f"{board}: GP{gpio} is claimed by more than one function: {listed}")

    # The LED, where the board has it only on the wireless chip.
    led = (spec.get("pins") or {}).get("user_led")
    if board in BOARD_HEADERS and led is not None:
        header = read(os.path.join(SDK, "src", "boards", "include", "boards", BOARD_HEADERS[board]))
        match = re.search(r"#define\s+CYW43_WL_GPIO_LED_PIN\s+(\d+)", header)
        default_led = re.search(r"#define\s+PICO_DEFAULT_LED_PIN\s+(\d+)", header)
        if match and not default_led:
            if "gpio" in led:
                fail(f"{board}: user_led is declared on GP{led['gpio']}, but the SDK board header has no "
                     f"PICO_DEFAULT_LED_PIN: the LED is CYW43_WL_GPIO_LED_PIN on the wireless chip")
            elif int(led.get("cyw43_gpio", -1)) != int(match.group(1)):
                fail(f"{board}: user_led.cyw43_gpio is {led.get('cyw43_gpio')}, the SDK's CYW43_WL_GPIO_LED_PIN is {match.group(1)}")

    # module.yml says the same.
    board_section = manifest.get("board") or {}
    yaml_pins = board_section.get("pins") or {}
    for name, pin in (spec.get("pins") or {}).items():
        other = yaml_pins.get(name)
        if other is None:
            fail(f"{board}: module.yml board.pins has no {name}")
            continue
        for key in ("gpio", "cyw43_gpio"):
            if pin.get(key) != other.get(key):
                fail(f"{board}: {name}.{key} is {pin.get(key)} in target.json and {other.get(key)} in module.yml")
    for name in yaml_pins:
        if name not in (spec.get("pins") or {}):
            fail(f"{board}: module.yml board.pins has {name}, which target.json does not")
    listed = [p.get("name") for p in (board_section.get("peripherals") or [])]
    for name in sorted({n for n in listed if listed.count(n) > 1}):
        fail(f"{board}: module.yml lists peripheral {name} {listed.count(name)} times")
    for name in spec.get("peripherals") or {}:
        if name not in listed:
            fail(f"{board}: module.yml does not list peripheral {name}")

    # The board support package compiles the same numbers.
    bsp_h = os.path.join(board_dir, "bsp", "bsp.h")
    bsp_c = os.path.join(board_dir, "bsp", "bsp.c")
    if os.path.exists(bsp_h):
        macros = {name: int(value) for name, value in
                  re.findall(r"#define\s+(RH_\w+_PIN)\s+\(?(-?\d+)\)?", read(bsp_h))}
        expected = {
            "RH_CONSOLE_TX_PIN": "uart0_tx", "RH_CONSOLE_RX_PIN": "uart0_rx",
            "RH_UART1_TX_PIN": "uart1_tx", "RH_UART1_RX_PIN": "uart1_rx",
            "RH_I2C0_SDA_PIN": "i2c0_sda", "RH_I2C0_SCL_PIN": "i2c0_scl",
            "RH_I2C1_SDA_PIN": "i2c1_sda", "RH_I2C1_SCL_PIN": "i2c1_scl",
            "RH_SPI0_TX_PIN": "spi0_tx", "RH_SPI0_RX_PIN": "spi0_rx",
            "RH_SPI0_SCK_PIN": "spi0_sck", "RH_SPI0_CSN_PIN": "spi0_csn",
            "RH_ONEWIRE0_PIN": "onewire0_dq", "RH_DHT0_PIN": "dht0_data",
        }
        for macro, name in expected.items():
            pin = (spec.get("pins") or {}).get(name)
            if macro in macros and pin is None:
                fail(f"{board}: bsp.h defines {macro} but target.json has no pins.{name}")
            elif pin is not None and macros.get(macro) != pin.get("gpio"):
                fail(f"{board}: bsp.h {macro} is {macros.get(macro)}, target.json pins.{name} is {pin.get('gpio')}")
    if os.path.exists(bsp_c):
        source = read(bsp_c)
        for array, prefix in (("pwm0_pins", "pwm0_ch"), ("adc0_pins", "adc0_ch")):
            match = re.search(array + r"\[\]\s*=\s*\{([^}]*)\}", source)
            if not match:
                continue
            compiled = [int(v) for v in re.findall(r"(\d+)u?", match.group(1))]
            declared = [int(pin["gpio"]) for name, pin in sorted((spec.get("pins") or {}).items())
                        if name.startswith(prefix)]
            if compiled != declared:
                fail(f"{board}: bsp.c {array} is {compiled}, target.json declares {declared}")


def main():
    routes = funcsel()
    boards = sorted(os.path.dirname(p) for p in glob.glob(os.path.join(ROOT, "boards", "*", "target.json")))
    if not boards:
        fail("no board under boards/ has a target.json")
    for board_dir in boards:
        check_board(board_dir, routes)
    if FAILURES:
        for message in FAILURES:
            print("FAIL:", message)
        return 1
    print(f"ok: {len(boards)} board spec(s), no pin claimed twice")
    return 0


if __name__ == "__main__":
    sys.exit(main())
