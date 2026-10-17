# SPDX-License-Identifier: GPL-3.0-or-later
"""Pool DOS-build adapter for a read-only, bounded RNG laboratory.

No original instruction bytes are distributed here. They come from an explicitly
supplied, fingerprinted private load image. Other games need a separate adapter.
"""
import ctypes
import hashlib
import os
import json
import subprocess
import sys
from pathlib import Path
import struct
import tempfile
import time

from compare_process import OwnedProcess


class PoolDosRandom:
    multiplier = 0x08088405
    increment = 1
    image_sha256 = "a42a57e68d3f7815831821cc9365bb917befda45a58e958544451fc7a878df8e"
    start_sha256 = "d825df2b174675c9088ba1489488bdeebe66ad2a22943f17d3a198e60b6a07bd"

    def __init__(self, image_path, dependency=None):
        path = Path(image_path).resolve(strict=True)
        self.image_path = path
        self.dependency = dependency
        if path.stat().st_size != 68592:
            raise ValueError("reference load image has an unsupported size")
        self.image = path.read_bytes()
        if hashlib.sha256(self.image).hexdigest() != self.image_sha256:
            raise ValueError("reference load image fingerprint is unsupported")

    def rolls(self, seed, bound, count):
        command = [sys.executable, str(Path(__file__).with_name('rng_original_probe.py')),
                   str(self.image_path), str(seed), str(bound), str(count), self.dependency or '']
        result = subprocess.run(command, capture_output=True, text=True, timeout=12)
        if result.returncode:
            raise RuntimeError(result.stderr.strip()[-1000:] or 'original probe failed')
        return json.loads(result.stdout)

    def seed_candidates(self, data, address, read):
        """Locate an unchanged runtime body, then verify relocated resident fields.

        The process may contain incidental copies: require the DOS PSP and both
        relocated segment references, and reject multiple validated candidates.
        """
        signature = self.image[0xc18f:0xc1c5]
        found = []
        offset = data.find(signature)
        while offset >= 0:
            base = address + offset - 0xc18f
            if base > 256:
                first = read(base + 6, 5)
                init = read(base + 0xaf80, 5)
                psp = read(base - 256, 2)
                if first[:1] == b'\x9a' and len(init) == 5 and psp == b'\xcd\x20':
                    system = int.from_bytes(first[3:5], 'little')
                    group = int.from_bytes(init[1:3], 'little')
                    if (init[0] == 0xba and init[3:] == b'\x8e\xda'
                            and system > 0xaf8 and group - system == 0x184):
                        seed_at = base + 0x10be8
                        a, b = read(seed_at, 4), read(seed_at, 4)
                        if len(a) == 4 and a == b:
                            found.append((seed_at, int.from_bytes(a, 'little')))
            offset = data.find(signature, offset + 1)
        return found

    def capture(self, dosbox):
        if os.name != 'nt':
            raise RuntimeError("live seed capture currently requires native Windows DOSBox")
        exe = Path(dosbox).resolve(strict=True)
        root = exe.parent
        game = (root / 'GAME').resolve(strict=True)
        start = game / 'POOLRAD' / 'START.EXE'
        repository = Path(__file__).resolve().parents[1]
        if (exe.name.lower() != 'dosbox.exe' or root.is_relative_to(repository)
                or 'steamapps' in str(root).lower() or 'program files' in str(root).lower()):
            raise ValueError("use the harness's private DOSBox copy outside the repository and installation")
        if not game.is_relative_to(root) or not start.resolve(strict=True).is_relative_to(game):
            raise ValueError('private game copy must remain inside the private reference directory')
        if start.stat().st_size != 65231 or hashlib.sha256(start.read_bytes()).hexdigest() != self.start_sha256:
            raise ValueError("private game copy has an unsupported START.EXE fingerprint")
        if '"' in str(game) or '\n' in str(game):
            raise ValueError("unsupported mount path")
        # Create a bounded configuration for the private copy, not the installed game.
        with tempfile.NamedTemporaryFile(mode='w', suffix='.conf', dir=root,
                                         encoding='ascii', delete=False) as config:
            config.write('[sdl]\nfullscreen=false\nautolock=false\noutput=surface\n'
                         '[dosbox]\nmemsize=16\n[cpu]\ncore=normal\ncycles=fixed 10000\n'
                         '[mixer]\nnosound=true\n[autoexec]\n@echo off\n'
                         f'mount c "{game}"\nc:\ncd poolrad\n@start\n')
        owned = None
        try:
            owned = OwnedProcess.launch([str(exe), '-conf', config.name, '-noconsole'], root)
            memory = WindowsMemory(owned.process)
            deadline = time.monotonic() + 10
            while time.monotonic() < deadline and owned.process.poll() is None:
                candidates = memory.find(self, deadline)
                if len(candidates) > 1:
                    raise RuntimeError("ambiguous guest-memory matches; refusing to choose a seed")
                # The unpacked resident image appears before startup seeds it.
                # Wait rather than presenting its initial zero-filled global as
                # an observed initialized stream. Midnight-zero is inconclusive.
                if candidates and candidates[0][1] != 0:
                    return dict(seed=candidates[0][1], pid=owned.process.pid,
                                source='live DOSBox read-only snapshot; original runtime replay is isolated')
                time.sleep(0.25)
            raise TimeoutError("no validated nonzero DOS RNG state found within ten seconds")
        finally:
            try:
                if owned is not None:
                    owned.kill_tree()
            finally:
                Path(config.name).unlink(missing_ok=True)


class WindowsMemory:
    """Read-only access to an already-owned child handle; never opens a PID."""
    def __init__(self, process):
        if os.name != 'nt':
            raise RuntimeError('Windows process memory is unavailable')
        self.process = process
        self.kernel = ctypes.WinDLL('kernel32', use_last_error=True)
        self.kernel.ReadProcessMemory.argtypes = [ctypes.c_void_p, ctypes.c_void_p,
            ctypes.c_void_p, ctypes.c_size_t, ctypes.POINTER(ctypes.c_size_t)]
        self.kernel.ReadProcessMemory.restype = ctypes.c_int
        self.kernel.VirtualQueryEx.argtypes = [ctypes.c_void_p, ctypes.c_void_p,
                                               ctypes.c_void_p, ctypes.c_size_t]
        self.kernel.VirtualQueryEx.restype = ctypes.c_size_t

    def read(self, address, length):
        result = ctypes.create_string_buffer(length)
        actual = ctypes.c_size_t()
        if not self.kernel.ReadProcessMemory(int(self.process._handle), address,
                result, length, ctypes.byref(actual)) or actual.value != length:
            return b''
        return result.raw

    def find(self, adapter, deadline):
        class Region(ctypes.Structure):
            _fields_ = [('base', ctypes.c_void_p), ('allocation', ctypes.c_void_p),
                        ('allocation_protect', ctypes.c_uint32), ('size', ctypes.c_size_t),
                        ('state', ctypes.c_uint32), ('protect', ctypes.c_uint32),
                        ('kind', ctypes.c_uint32)]
        address = 0
        scanned = 0
        candidates = set()
        # The supported native DOSBox is 32-bit. Cap both readable bytes and time.
        while address < 0x80000000 and scanned < 256 * 1024 * 1024:
            if time.monotonic() >= deadline or self.process.poll() is not None:
                break
            region = Region()
            if not self.kernel.VirtualQueryEx(int(self.process._handle), address,
                                              ctypes.byref(region), ctypes.sizeof(region)):
                break
            base = region.base or 0
            following = base + region.size
            if following <= address:
                break
            if (region.state == 0x1000 and region.kind == 0x20000
                    and not region.protect & 0x101 and region.protect & 0xee):
                for chunk in range(base, following, 1024 * 1024):
                    if time.monotonic() >= deadline or scanned >= 256 * 1024 * 1024:
                        break
                    length = min(1024 * 1024 + 64, following - chunk)
                    data = self.read(chunk, length)
                    scanned += length
                    candidates.update(adapter.seed_candidates(data, chunk, self.read))
            address = following
        return sorted(candidates)
