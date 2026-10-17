#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Redistributable RNG laboratory tests; no original binaries or Unicorn needed."""
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
import rng_debug
from rng_reference import PoolDosRandom

PROBE = sys.argv.pop(1)


class RandomTests(unittest.TestCase):
    def probe(self, seed, bound, count, multiplier=0x08088405, increment=1):
        result = subprocess.run([PROBE, *map(str, [seed, bound, count, multiplier, increment])],
                                capture_output=True, text=True, timeout=5, check=True)
        return json.loads(result.stdout)

    def test_original_observed_vector_and_resume(self):
        rows = self.probe(0x12345678, 20, 10)
        self.assertEqual([r['value'] for r in rows], [19, 11, 16, 4, 9, 19, 8, 3, 12, 7])
        resumed = self.probe(rows[4]['after'], 20, 5)
        self.assertEqual([r['value'] for r in resumed], [r['value'] for r in rows[5:]])
        self.assertEqual([r['after'] for r in resumed], [r['after'] for r in rows[5:]])

    def test_zero_bound_advances_and_parameters_are_generic(self):
        zero = self.probe(0x12345678, 0, 3)
        normal = self.probe(0x12345678, 20, 3)
        self.assertEqual([r['value'] for r in zero], [0, 0, 0])
        self.assertEqual([r['after'] for r in zero], [r['after'] for r in normal])
        generic = self.probe(0xffffffff, 65535, 2, 1, 1)
        self.assertEqual([r['after'] for r in generic], [0, 1])

    def test_invalid_probe_inputs_fail(self):
        for args in [('0', '20', '65'), ('-1', '20', '1'), ('0', '65536', '1'), ('0', '1', '0')]:
            result = subprocess.run([PROBE, *args, '1', '1'], capture_output=True, timeout=5)
            self.assertNotEqual(result.returncode, 0)

    def test_mismatch_and_missing_rows_are_not_passes(self):
        row = dict(index=1, before=1, value=2, after=3)
        class Reference:
            def rolls(self, *args):
                return [row]
        with patch.object(rng_debug, 'engine_rolls', return_value=[dict(row, after=4)]):
            report = rng_debug.compare(Reference(), PROBE, 1, 20, 1)
            self.assertFalse(report['match'])
        with patch.object(rng_debug, 'engine_rolls', return_value=[]):
            with self.assertRaises(ValueError):
                rng_debug.compare(Reference(), PROBE, 1, 20, 1)
        with self.assertRaises(ValueError):
            rng_debug.compare(Reference(), PROBE, 1, 20, 0)

    def test_unknown_original_image_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            image = Path(directory) / 'synthetic.bin'
            image.write_bytes(bytes(68592))
            with self.assertRaisesRegex(ValueError, 'fingerprint'):
                PoolDosRandom(image)

    def test_seed_locator_checks_relocation_and_psp(self):
        adapter = PoolDosRandom.__new__(PoolDosRandom)
        image = bytearray(68592)
        image[0xc18f:0xc1c5] = bytes(range(54))
        adapter.image = bytes(image)
        memory = bytearray(0x50000)
        base = 0x20000
        memory[base:base + len(image)] = image
        memory[base - 256:base - 254] = b'\xcd\x20'
        memory[base + 6:base + 11] = b'\x9a' + struct.pack('<HH', 0, 0x1af8)
        memory[base + 0xaf80:base + 0xaf85] = b'\xba' + struct.pack('<H', 0x1c7c) + b'\x8e\xda'
        memory[base + 0x10be8:base + 0x10bec] = struct.pack('<I', 0x12345678)
        def read(address, size):
            return bytes(memory[address:address + size])
        self.assertEqual(adapter.seed_candidates(bytes(memory), 0, read), [(base + 0x10be8, 0x12345678)])
        memory[base - 256] = 0
        self.assertEqual(adapter.seed_candidates(bytes(memory), 0, read), [])


if __name__ == '__main__':
    unittest.main()
