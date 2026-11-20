#!/bin/sh
# flash_ab.sh — flash a Pico 2 W for updates over the link (PICO-0012, robohub
# ADR-0023): the A/B partition table, then the program into partition A, then
# run it. The board is reached over USB by picotool: a running program is asked
# to reboot into BOOTSEL, and the board stays there until the program is in.
#
#   tools/flash_ab.sh path/to/program.elf
#
# @project RoboHub Platform
# @license MIT
set -eu
image="${1:?usage: flash_ab.sh program.elf}"
here="$(cd "$(dirname "$0")/.." && pwd)"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

# Until the board answers in BOOTSEL: a reboot re-enumerates it, which takes a
# few seconds (longer through a USB forwarder such as usbipd).
wait_bootsel() {
    tries=0
    until picotool info >/dev/null 2>&1; do
        tries=$((tries + 1))
        if [ "$tries" -gt 40 ]; then
            echo "flash: the board did not come back in BOOTSEL" >&2
            exit 1
        fi
        sleep 1
    done
}

picotool partition create --quiet "$here/partitions/ab.json" "$work/pt.uf2"
picotool reboot -f -u >/dev/null 2>&1 || true
wait_bootsel
echo "flash: the A/B partition table"
picotool load "$work/pt.uf2"
# The boot ROM reads the partition table when it starts: into BOOTSEL again, so
# the program goes into the partition the table now defines.
picotool reboot -u >/dev/null 2>&1 || true
sleep 2
wait_bootsel
echo "flash: the program into partition A"
picotool load -p 0 -x "$image"
