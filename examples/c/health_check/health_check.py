#!/usr/bin/env python3
"""On-target health check for a Pico 2 W (PICO-0010, PICO-0014).

Run with the Pico 2 W on USB (and, under WSL, kept attached across its resets,
e.g. `usbipd attach --wsl --busid <id> --auto-attach`):

    python3 health_check.py

Builds this example, flashes it, and reads the board's USB console across its
three boots — a watchdog reset, a recorded fault, then a fed watchdog, with the
keystore holding a key across each reset — until it prints `health-check:
passed` or a failure. The keystore then holds the check's keys: flash the board
from Mesh afterwards.
"""
import glob
import os
import subprocess
import sys
import time

import serial

HERE = os.path.dirname(os.path.abspath(__file__))
PICO = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
WORKSPACE = os.environ.get("ROBOHUB_WORKSPACE", os.path.dirname(PICO))


def main():
    env = dict(os.environ, ROBOHUB_WORKSPACE=WORKSPACE)
    for step in (["rho", "build", "-target", "pico2w", "."],
                 ["rho", "flash", "-target", "pico2w", "build/output/health_check.elf"]):
        done = subprocess.run(step, cwd=HERE, env=env)
        if done.returncode != 0:
            print("FAILED:", " ".join(step))
            return 1
    end = time.time() + 120
    port, line = None, b""
    while time.time() < end:
        if port is None:
            for path in sorted(glob.glob("/dev/ttyACM*")):
                try:
                    port = serial.Serial(path, 115200, timeout=0.2)
                    port.dtr = True
                    break
                except (serial.SerialException, OSError):
                    port = None
            if port is None:
                time.sleep(0.3)
                continue
        try:
            data = port.read(256)
        except (serial.SerialException, OSError):
            port = None  # the board reset; its USB comes back
            continue
        for byte in data:
            if byte == 0x0A:
                text = line.decode("utf-8", "replace").rstrip()
                print(text, flush=True)
                if text.startswith("health-check: passed"):
                    print("OK: the health contracts and the keystore held on the board")
                    return 0
                if "FAILED" in text:
                    return 1
                line = b""
            else:
                line += bytes([byte])
    print("FAILED: no verdict within 120 s")
    return 1


if __name__ == "__main__":
    sys.exit(main())
