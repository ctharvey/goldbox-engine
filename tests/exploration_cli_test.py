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

    # Location-only events still run on cells with selector zero. CMP2 checks
    # X=1/Y=0; IF_NE exits elsewhere. Movement, blocked input, re-entry and save
    # restoration use the real CLI rather than invoking the VM directly.
    (root / "GEO.DAX").write_bytes(dax(bytes(1026)))
    location_event = bytes([1, 1, 20, 0x99] * 5 + [
        0x14, 1, 0x4b, 0xc0, 0, 1, 1, 0x4c, 0xc0, 0, 0,
        0x17, 0,
        4, 1, 0, 0x4a, 0, 1, 1, 0, 0x4a,
        0x11, 0x80, 1, 4, 0])
    (root / "ECL.DAX").write_bytes(dax(location_event))
    location_save = root / "location.json"
    location_command = command[:-1] + [str(location_save)]
    walked = subprocess.run(location_command, input="east\nnorth\neast\nwest\nsave\nquit\n",
                            text=True, capture_output=True, timeout=30)
    assert walked.returncode == 0, walked.stdout + walked.stderr
    assert walked.stdout.count("> A\n") == 2, walked.stdout
    location = json.loads(location_save.read_text())
    assert location["row"] == 0 and location["column"] == 1 and location["memory"][0x4a00] == 2
    assert location["memory"][0xc04f] == 0
    reloaded = subprocess.run(location_command, input="load\neast\nwest\nquit\n",
                              text=True, capture_output=True, timeout=30)
    assert reloaded.returncode == 0 and reloaded.stdout.count("> A\n") == 1, reloaded.stdout + reloaded.stderr
