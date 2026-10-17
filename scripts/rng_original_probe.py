#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Independently written standalone analysis tool; not linked into the GPLv3 engine.
# Optional Unicorn is an external GPLv2 analysis tool, not distributed with this script.
# No upstream implementation or original game bytes copied.
# Attribution-ID: unicorn-analysis-tool
import hashlib
import json
from pathlib import Path
import struct
import sys
import time


def rolls(image, seed, bound, count):
    from unicorn import Uc, UC_ARCH_X86, UC_MODE_16
    from unicorn.x86_const import (
        UC_X86_REG_CS, UC_X86_REG_DS, UC_X86_REG_SS, UC_X86_REG_SP, UC_X86_REG_AX)
    cpu = Uc(UC_ARCH_X86, UC_MODE_16)
    cpu.mem_map(0, 0x100000)
    cpu.mem_write(0x20000, image)
    seed_address = 0x20000 + 0x10be8
    cpu.mem_write(seed_address, struct.pack('<I', seed))
    rows = []
    deadline = time.monotonic() + 10
    for index in range(count):
        if time.monotonic() >= deadline:
            raise TimeoutError("original-code comparison exceeded ten seconds")
        before = int.from_bytes(cpu.mem_read(seed_address, 4), 'little')
        cpu.reg_write(UC_X86_REG_CS, 0x2af8)
        cpu.reg_write(UC_X86_REG_DS, 0x2c7c)
        cpu.reg_write(UC_X86_REG_SS, 0x8000)
        cpu.reg_write(UC_X86_REG_SP, 0xff00)
        cpu.mem_write(0x8ff00, struct.pack('<HHH', 0x1000, 0x9000, bound))
        cpu.emu_start(0x2c140, 0x91000, timeout=100000, count=2000)
        if cpu.reg_read(UC_X86_REG_CS) != 0x9000:
            raise RuntimeError("original routine did not return within its budget")
        rows.append(dict(index=index + 1, before=before,
                         value=cpu.reg_read(UC_X86_REG_AX),
                         after=int.from_bytes(cpu.mem_read(seed_address, 4), 'little')))
    return rows


if __name__ == '__main__':
    try:
        image_path, seed, bound, count, dependency = sys.argv[1:]
        if dependency:
            sys.path.insert(0, str(Path(dependency).resolve(strict=True)))
        seed, bound, count = int(seed), int(bound), int(count)
        if not 0 <= seed <= 0xffffffff or not 0 <= bound <= 65535 or not 1 <= count <= 64:
            raise ValueError('invalid bounded probe arguments')
        path = Path(image_path)
        if path.stat().st_size != 68592:
            raise ValueError('unsupported original image size')
        image = path.read_bytes()
        if hashlib.sha256(image).hexdigest() != 'a42a57e68d3f7815831821cc9365bb917befda45a58e958544451fc7a878df8e':
            raise ValueError('unsupported original image fingerprint')
        print(json.dumps(rolls(image, seed, bound, count)))
    except Exception as error:
        print(f'original RNG probe: {error}', file=sys.stderr)
        raise SystemExit(1)
