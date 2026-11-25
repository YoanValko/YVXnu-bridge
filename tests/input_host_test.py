#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Validate the hosted input writer without loading a kernel module."""

import struct
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
INPUT = ROOT / "tools" / "hosted" / "xnu-input.py"
EVENT = struct.Struct("<HHi")


def read_events(path):
    data = path.read_bytes()
    if len(data) % EVENT.size:
        raise AssertionError(f"partial event record: {len(data)} bytes")
    return [EVENT.unpack_from(data, i) for i in range(0, len(data), EVENT.size)]


def main():
    with tempfile.NamedTemporaryFile(prefix="yvxnu-input-", delete=False) as stream:
        path = Path(stream.name)

    try:
        subprocess.run(
            [
                sys.executable,
                str(INPUT),
                "--file",
                str(path),
                "--screen",
                "100x50",
                "move",
                "-10",
                "999",
                "click",
                "25",
                "10",
                "type",
                "A",
                "key",
                "return",
            ],
            check=True,
        )
        events = read_events(path)
    finally:
        path.unlink(missing_ok=True)

    if not events:
        raise AssertionError("input writer produced no events")
    if events[0] != (3, 0, 0) or events[1] != (3, 1, 32767):
        raise AssertionError(f"screen-coordinate clamping failed: {events[:2]}")
    if (1, 0x110, 1) not in events or (1, 0x110, 0) not in events:
        raise AssertionError("left-click press/release missing")
    if (1, 42, 1) not in events or (1, 30, 1) not in events:
        raise AssertionError("shift+A keyboard sequence missing")
    if (1, 28, 1) not in events or (1, 28, 0) not in events:
        raise AssertionError("return key sequence missing")
    print("input_host_test: PASS")


if __name__ == "__main__":
    main()
