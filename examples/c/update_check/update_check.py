#!/usr/bin/env python3
"""On-target update check for a Pico 2 W (PICO-0011, robohub ADR-0023).

Run with the Pico 2 W on USB (and, under WSL, kept attached across its reboots,
e.g. `usbipd attach --wsl --busid <id> --auto-attach`):

    python3 update_check.py

1. builds this example three times — version 1 (flashed), version 2 (an update
   image, try-before-you-buy), version 3 (an update image that hangs on trial);
2. flashes version 1 with the A/B partition table (tools/flash_ab.sh);
3. updates it over the USB link to version 2: verified, booted once, confirmed,
   and version 2 runs from partition B;
   The transfer is cut halfway: the check stops sending, offers the update
   again, and continues from the chunk the board says it has (PICO-0013);
4. offers an image whose security version is below the board's, which the board
   refuses as a downgrade (PICO-0013);
5. updates it to version 3, which hangs on trial: the boot ROM's watchdog
   reverts to version 2, which reports version 3 rolled back.

The board's USB serial port carries the link's frames only. The update key is
the test key whose seed is the bytes 0..31 (src/update_key.h).
"""
import glob
import hashlib
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import time

import serial
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey

HERE = os.path.dirname(os.path.abspath(__file__))
PICO = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
WORKSPACE = os.environ.get("ROBOHUB_WORKSPACE", os.path.dirname(PICO))
KEY = Ed25519PrivateKey.from_private_bytes(bytes(range(32)))
UID = bytes([0x55, 0x50, 0x44, 0x41, 0x54, 0x45, 0x00, 0x01])
OFFER, CHUNK, STATUS, HEALTH = 0x0501, 0x0502, 0x0503, 0x0403
ACCEPTED, REFUSED, RECEIVING, VERIFIED, CONFIRMED, ROLLED_BACK, FAILED = range(1, 8)
DOWNGRADE = 3
CHUNK_SIZE = 192
failures = []


def check(ok, what):
    print(("[PASS] " if ok else "[FAIL] ") + what, flush=True)
    if not ok:
        failures.append(what)


def fletcher16(data):
    a = b = 0
    for x in data:
        a = (a + x) % 255
        b = (b + a) % 255
    return (b << 8) | a


def cobs_encode(data):
    out, block = bytearray(), bytearray()
    for x in data:
        if x == 0:
            out += bytes([len(block) + 1]) + block
            block = bytearray()
        else:
            block.append(x)
            if len(block) == 254:
                out += bytes([255]) + block
                block = bytearray()
    out += bytes([len(block) + 1]) + block
    return bytes(out)


def cobs_decode(data):
    out, i = bytearray(), 0
    while i < len(data):
        code = data[i]
        i += 1
        out += data[i:i + code - 1]
        i += code - 1
        if code != 255 and i < len(data):
            out.append(0)
    return bytes(out)


def frame(msg_id, payload):
    body = struct.pack("<HHH", msg_id, 0, len(payload)) + payload
    return cobs_encode(body + struct.pack("<H", fletcher16(body))) + b"\x00"


class Link:
    """The board's USB serial port, opened again whenever the board comes back."""

    def __init__(self):
        self.port = None
        self.buf = bytearray()

    def open(self, timeout=40.0):
        end = time.time() + timeout
        while time.time() < end:
            for path in sorted(glob.glob("/dev/ttyACM*")):
                try:
                    self.port = serial.Serial(path, 115200, timeout=0.05)
                    self.port.dtr = True
                    self.buf = bytearray()
                    return True
                except (serial.SerialException, OSError):
                    pass
            time.sleep(0.3)
        return False

    def close(self):
        if self.port:
            try:
                self.port.close()
            except Exception:  # noqa: BLE001
                pass
        self.port = None

    def send(self, msg_id, payload):
        self.port.write(frame(msg_id, payload))

    def frames(self):
        try:
            data = self.port.read(4096)
        except (serial.SerialException, OSError):
            self.close()
            return []
        self.buf += data
        out = []
        while b"\x00" in self.buf:
            raw, _, rest = self.buf.partition(b"\x00")
            self.buf = bytearray(rest)
            body = cobs_decode(bytes(raw))
            if len(body) >= 8:
                msg_id, _tx, length = struct.unpack_from("<HHH", body)
                out.append((msg_id, body[6:6 + length]))
        return out

    def wait(self, test, timeout):
        """The first status or health frame `test` accepts; reopens after a reboot."""
        end = time.time() + timeout
        while time.time() < end:
            if self.port is None:
                if not self.open(max(1.0, end - time.time())):
                    return None
            for msg_id, payload in self.frames():
                if msg_id == STATUS and len(payload) == 20:
                    s = struct.unpack("<BIIIIHB", payload)[1:]
                    print("  status", s, flush=True)
                    if test(("status",) + s):
                        return ("status",) + s
                elif msg_id == HEALTH and len(payload) == 30:
                    fw = struct.unpack_from("<H", payload, 28)[0]
                    print("  health, firmware", fw, flush=True)
                    if test(("health", fw)):
                        return ("health", fw)
        return None


def build(work, version, update, hang):
    conf = os.path.join(HERE, "project.conf")
    base = open(conf).read()
    try:
        extra = f"CONFIG_UPDATE_CHECK_VERSION={version}\n"
        if update:
            extra += "CONFIG_RH_UPDATE_IMAGE=y\n"
        if hang:
            extra += "CONFIG_UPDATE_CHECK_HANG=y\n"
        open(conf, "w").write(base + extra)
        shutil.rmtree(os.path.join(HERE, "build"), ignore_errors=True)
        env = dict(os.environ, ROBOHUB_WORKSPACE=WORKSPACE)
        done = subprocess.run(["rho", "build", "-target", "pico2w", "."], cwd=HERE, env=env,
                              capture_output=True, text=True)
        if done.returncode != 0:
            raise SystemExit("build failed:\n" + done.stdout[-2000:] + done.stderr[-2000:])
        out = os.path.join(HERE, "build", "output")
        name = f"v{version}"
        shutil.copy(os.path.join(out, "update_check.elf"), os.path.join(work, name + ".elf"))
        shutil.copy(os.path.join(out, "update_check.bin"), os.path.join(work, name + ".bin"))
        return os.path.join(work, name + ".elf"), open(os.path.join(work, name + ".bin"), "rb").read()
    finally:
        open(conf, "w").write(base)


def offer(update_id, image, firmware, security=1):
    manifest = struct.pack("<BI8sHII32sH", 1, update_id, UID, firmware, security, len(image),
                           hashlib.sha256(image).digest(), CHUNK_SIZE)
    return b"\x01" + manifest + KEY.sign(b"rh-update1" + manifest)


def resume(link, update_id, image, firmware, cut_at):
    """A transfer cut at `cut_at`: nothing is sent for three seconds, then the
    update is offered again and the board names the chunk it continues from."""
    end = time.time() + 3.0
    while time.time() < end:
        link.frames()
        time.sleep(0.05)
    link.send(OFFER, offer(update_id, image, firmware))
    s = link.wait(lambda s: s[0] == "status" and s[1] == update_id and s[2] == ACCEPTED, 10)
    nxt = s[4] if s is not None else -1
    check(s is not None and cut_at - 4 <= nxt <= cut_at,
          f"after a cut at chunk {cut_at} the board takes the offer again and resumes at {nxt}")
    return nxt


def update(link, update_id, image, firmware, cut_at=None):
    """Offer, then every chunk with up to four in flight; True when verified.
    With `cut_at`, the transfer stops there once and is resumed (PICO-0013)."""
    link.send(OFFER, offer(update_id, image, firmware))
    s = link.wait(lambda s: s[0] == "status" and s[1] == update_id, 10)
    check(s is not None and s[2] == ACCEPTED, f"update {update_id} is accepted ({s})")
    if s is None or s[2] != ACCEPTED:
        return False
    chunks = (len(image) + CHUNK_SIZE - 1) // CHUNK_SIZE
    acked = sent = 0
    last = progress = time.time()
    while True:
        if time.time() - progress > 60:
            print(f"  no progress for 60 s at chunk {acked}/{chunks}", flush=True)
            return False
        for msg_id, payload in link.frames():
            if msg_id == STATUS and len(payload) == 20:
                _v, uid, state, reason, nxt, _ver, _slot = struct.unpack("<BIIIIHB", payload)
                if uid != update_id:
                    continue
                if state == VERIFIED:
                    return True
                if state in (FAILED, REFUSED):
                    print("  status", (uid, state, reason, nxt), flush=True)
                    return False
                if nxt != acked or nxt < sent:
                    if nxt > acked:
                        progress = time.time()
                    acked = nxt
                    sent = min(sent, acked + 4) if nxt >= sent - 4 else acked
                    if nxt < sent:
                        sent = nxt
                    last = time.time()
        if cut_at is not None and sent == cut_at:
            nxt = resume(link, update_id, image, firmware, cut_at)
            cut_at = None
            if nxt < 0:
                return False
            acked = sent = nxt
            last = progress = time.time()
            continue
        if sent < chunks and sent < acked + 4:
            link.send(CHUNK, struct.pack("<II", update_id, sent) + image[sent * CHUNK_SIZE:(sent + 1) * CHUNK_SIZE])
            sent += 1
            continue
        if time.time() - last > 2.0:
            sent = acked
            last = time.time()
        if link.port is None:
            # Gone after the last chunk: the board verified the image and
            # rebooted into it, and its `verified` went with its USB.
            return sent >= chunks


def outcome(link, update_id, image, firmware, timeout):
    """How an update ended, asked as a gateway asks: a board whose report went
    while its USB was away answers an offer of the update with its outcome."""
    end = time.time() + timeout
    while time.time() < end:
        s = link.wait(lambda s: s[0] == "status" and s[1] == update_id and s[2] in (CONFIRMED, ROLLED_BACK, FAILED),
                      min(12, max(1, end - time.time())))
        if s is not None:
            return s
        if link.port is not None:
            try:
                link.send(OFFER, offer(update_id, image, firmware))
            except (serial.SerialException, OSError):
                link.close()
    return None


def main():
    work = tempfile.mkdtemp(prefix="rh-update-check-")
    print("building version 1, 2 and 3 ...", flush=True)
    v1, _ = build(work, 1, False, False)
    _, image2 = build(work, 2, True, False)
    _, image3 = build(work, 3, True, True)
    print(f"images: {len(image2)} bytes; flashing version 1 with the A/B table", flush=True)
    flashed = subprocess.run(["sh", os.path.join(PICO, "tools", "flash_ab.sh"), v1])
    check(flashed.returncode == 0, "version 1 is flashed with the A/B partition table")
    link = Link()
    up = link.wait(lambda s: s[0] == "health", 40)
    check(up == ("health", 1), f"version 1 runs and reports its health ({up})")

    # Version 2: cut halfway and resumed, then kept.
    if update(link, 200, image2, 2, cut_at=(len(image2) // CHUNK_SIZE) // 2):
        check(True, "update 200 (version 2) is verified; the board boots it once")
        link.close()
        s = outcome(link, 200, image2, 2, 90)
        check(s is not None and s[2] == CONFIRMED and s[5] == 2 and s[6] == 1,
              f"version 2 confirms itself from partition B ({s})")
    else:
        check(False, "update 200 (version 2) is verified")

    # A downgrade: security version 0 on a board that kept version 1's.
    link.send(OFFER, offer(202, image3, 3, security=0))
    s = link.wait(lambda s: s[0] == "status" and s[1] == 202, 10)
    check(s is not None and s[2] == REFUSED and s[3] == DOWNGRADE,
          f"an image below the board's security version is refused as a downgrade ({s})")

    # Version 3: hangs on trial, rolled back.
    if update(link, 201, image3, 3):
        check(True, "update 201 (version 3) is verified; the board boots it once")
        link.close()
        s = outcome(link, 201, image3, 3, 120)
        check(s is not None and s[2] == ROLLED_BACK and s[5] == 2 and s[6] == 1,
              f"version 3 hangs, the boot ROM goes back, and version 2 says rolled back ({s})")
    else:
        check(False, "update 201 (version 3) is verified")

    link.close()
    shutil.rmtree(work, ignore_errors=True)
    if failures:
        print("FAILED:", *failures, sep="\n  ")
        return 1
    print("OK: a Pico 2 W updated over its link — resumed a cut transfer, kept one image, "
          "refused a downgrade, rolled back another")
    return 0


if __name__ == "__main__":
    sys.exit(main())
