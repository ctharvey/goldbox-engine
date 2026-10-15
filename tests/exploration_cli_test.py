# SPDX-License-Identifier: GPL-3.0-or-later
"""Terminal movement/event/save/reload using only independently authored bytes."""
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile


def dax(raw):
    packed = b"".join(bytes([len(raw[i:i + 128]) - 1]) + raw[i:i + 128] for i in range(0, len(raw), 128))
    return struct.pack("<HBIHH", 9, 7, 0, len(raw), len(packed)) + packed


with tempfile.TemporaryDirectory() as temp:
    root = Path(temp)
    geo = bytearray(1026)
    geo[515] = 0x81  # row 0, column 1
    (root / "GEO.DAX").write_bytes(dax(geo))
    ecl = bytes([1, 1, 20, 0x99] * 5 + [9, 1, 0x4f, 0xc0, 1, 0, 0x4a, 0x11, 0x80, 1, 4, 0])
    (root / "ECL.DAX").write_bytes(dax(ecl))
    command = [sys.argv[1], sys.argv[2], str(root / "GEO.DAX"), "7", str(root / "ECL.DAX"), "7", str(root / "state.json")]
    first = subprocess.run(command, input="east\r\nsave\r\nquit\r\n", text=True, capture_output=True, timeout=30)
    if first.returncode != 0 or "State saved" not in first.stdout or "Event exited" not in first.stdout:
        raise RuntimeError(first.stdout + first.stderr)
    state = json.loads((root / "state.json").read_text())
    assert state["row"] == 0 and state["column"] == 1 and state["memory"][0x4a00] == 0x81
    second = subprocess.run(command, input="load\nquit\n", text=True, capture_output=True, timeout=30)
    assert second.returncode == 0 and "State restored" in second.stdout and "(0,1)>" in second.stdout
    overwrite = subprocess.run(command, input="save\n", text=True, capture_output=True, timeout=30)
    assert overwrite.returncode != 0 and "Refusing to overwrite" in overwrite.stderr
    changed = bytearray(geo)
    changed[515] = 2
    (root / "GEO.DAX").write_bytes(dax(changed))
    mismatch = subprocess.run(command, input="load\n", text=True, capture_output=True, timeout=30)
    assert mismatch.returncode != 0 and "source/dialect mismatch" in mismatch.stderr
