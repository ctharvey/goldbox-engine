#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Redistributable RNG laboratory tests; no original binaries or Unicorn needed."""
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
from types import SimpleNamespace
from unittest.mock import Mock, patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
import rng_debug
from rng_reference import PoolDosRandom
import rng_reference

PROBE = sys.argv.pop(1)


# Numeric observations from the fingerprinted original DOS runtime, not a Python RNG oracle.
# Load-image SHA-256: a42a57e68d3f7815831821cc9365bb917befda45a58e958544451fc7a878df8e
# Recorded with Unicorn 2.1.4. Only seeds, roll values and states are redistributed.
ORIGINAL_VECTORS = {
    0x00000000: (
        (0x00000001, 0x08088406, 0xdc6dac1f, 0x33dc589c, 0x45de2b0d, 0xabf18b42, 0x5195c04b, 0x296b6d78),
        {
            0: (0, 0, 0, 0, 0, 0, 0, 0),
            1: (0, 0, 0, 0, 0, 0, 0, 0),
            2: (0, 0, 1, 0, 0, 1, 1, 1),
            6: (0, 4, 5, 4, 0, 1, 5, 1),
            20: (0, 16, 9, 16, 6, 17, 5, 3),
            256: (0, 8, 109, 220, 222, 241, 149, 107),
            65535: (0, 2056, 56429, 13276, 17886, 44017, 20885, 10603),
        }),
    0x00000001: (
        (0x08088406, 0xdc6dac1f, 0x33dc589c, 0x45de2b0d, 0xabf18b42, 0x5195c04b, 0x296b6d78, 0x5f4b0359),
        {
            0: (0, 0, 0, 0, 0, 0, 0, 0),
            1: (0, 0, 0, 0, 0, 0, 0, 0),
            2: (0, 1, 0, 0, 1, 1, 1, 1),
            6: (4, 5, 4, 0, 1, 5, 1, 5),
            20: (16, 9, 16, 6, 17, 5, 3, 15),
            256: (8, 109, 220, 222, 241, 149, 107, 75),
            65535: (2056, 56429, 13276, 17886, 44017, 20885, 10603, 24395),
        }),
    0xffffffff: (
        (0xf7f77bfc, 0x33a35bed, 0x84feffa2, 0x21da862b, 0xdfcacad8, 0x064d5639, 0x79c0131e, 0xf38bd797),
        {
            0: (0, 0, 0, 0, 0, 0, 0, 0),
            1: (0, 0, 0, 0, 0, 0, 0, 0),
            2: (1, 1, 0, 0, 0, 1, 0, 1),
            6: (5, 1, 2, 2, 2, 5, 4, 1),
            20: (19, 19, 6, 6, 10, 13, 8, 7),
            256: (247, 163, 254, 218, 202, 77, 192, 139),
            65535: (63479, 13219, 34046, 8666, 57290, 1613, 31168, 62347),
        }),
    0x12345678: (
        (0xcb5b9059, 0x79ffb5be, 0xd9a484b7, 0xf25cf394, 0xe60911e5, 0xe28f6d7a, 0x9d100b63, 0xca4744f0),
        {
            0: (0, 0, 0, 0, 0, 0, 0, 0),
            1: (0, 0, 0, 0, 0, 0, 0, 0),
            2: (1, 1, 0, 0, 1, 1, 0, 1),
            6: (3, 1, 0, 4, 5, 3, 2, 3),
            20: (19, 11, 16, 4, 9, 19, 8, 3),
            256: (91, 255, 164, 92, 9, 143, 16, 71),
            65535: (52059, 31231, 55716, 62044, 58889, 57999, 40208, 51783),
        }),
    0x2e4d0932: (
        (0x79cef5fb, 0x9fb839e8, 0x2bb4c189, 0x629a6bae, 0x59f9d267, 0x5d963804, 0x28f12815, 0x0e089c6a),
        {
            0: (0, 0, 0, 0, 0, 0, 0, 0),
            1: (0, 0, 0, 0, 0, 0, 0, 0),
            2: (0, 0, 0, 0, 1, 0, 1, 0),
            6: (0, 4, 4, 0, 5, 0, 5, 4),
            20: (2, 8, 8, 2, 13, 18, 1, 12),
            256: (206, 184, 180, 154, 249, 150, 241, 8),
            65535: (31182, 40888, 11188, 25242, 23033, 23958, 10481, 3592),
        }),
}

class RandomTests(unittest.TestCase):
    def probe(self, seed, bound, count, multiplier=0x08088405, increment=1):
        result = subprocess.run([PROBE, *map(str, [seed, bound, count, multiplier, increment])],
                                capture_output=True, text=True, timeout=5, check=True)
        return json.loads(result.stdout)

    def test_original_matrix_values_and_every_state(self):
        for seed, (states, bounds) in ORIGINAL_VECTORS.items():
            for bound, values in bounds.items():
                with self.subTest(seed=hex(seed), bound=bound):
                    rows = self.probe(seed, bound, len(states))
                    expected = [dict(index=i + 1, before=before, value=value, after=after)
                                for i, (before, value, after) in enumerate(
                                    zip((seed, *states[:-1]), values, states))]
                    self.assertEqual(rows, expected)

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

    def test_private_regression_skips_absence_but_fails_partial_configuration(self):
        environment = {key: value for key, value in os.environ.items()
                       if not key.startswith('GOLDBOX_RNG_')}
        script = Path(__file__).with_name('rng_original_integration_test.py')
        skipped = subprocess.run([sys.executable, str(script), PROBE], env=environment,
                                 capture_output=True, text=True, timeout=5)
        self.assertEqual(skipped.returncode, 77)
        self.assertIn('SKIP:', skipped.stdout)
        environment['GOLDBOX_RNG_IMAGE'] = 'unavailable-private-image'
        partial = subprocess.run([sys.executable, str(script), PROBE], env=environment,
                                 capture_output=True, text=True, timeout=5)
        self.assertEqual(partial.returncode, 1)
        self.assertIn('partial configuration', partial.stderr)

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

    def test_each_value_and_state_difference_is_detected(self):
        row = dict(index=1, before=1, value=2, after=3)
        reference = Mock()
        reference.rolls.return_value = [row]
        for field in ('before', 'value', 'after'):
            with self.subTest(field=field):
                reference.reset_mock()
                with patch.object(rng_debug, 'engine_rolls', return_value=[dict(row, **{field: 4})]) as engine:
                    report = rng_debug.compare(reference, PROBE, 1, 20, 1)
                self.assertFalse(report['match'])
                reference.rolls.assert_called_once_with(1, 20, 1)
                engine.assert_called_once_with(PROBE, reference, 1, 20, 1)

    def test_capture_waits_for_initialization_and_always_cleans_up(self):
        # Synthetic files and mocked OS boundary: no native process or original
        # resource is needed to cover initialization, ambiguity and timeout.
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            exe = root / 'DOSBox.exe'
            exe.write_bytes(b'')
            game = root / 'GAME' / 'POOLRAD'
            game.mkdir(parents=True)
            (game / 'START.EXE').write_bytes(bytes(65231))
            adapter = PoolDosRandom.__new__(PoolDosRandom)
            for scenario in ('initialized', 'ambiguous', 'timeout'):
                owned = Mock()
                owned.process.pid = 123
                owned.process.poll.return_value = None
                memory = Mock()
                if scenario == 'initialized':
                    memory.find.side_effect = [[(100, 0)], [(100, 7)]]
                elif scenario == 'ambiguous':
                    memory.find.return_value = [(100, 7), (200, 8)]
                else:
                    memory.find.return_value = []
                with self.subTest(scenario=scenario), \
                        patch.object(rng_reference, 'os', SimpleNamespace(name='nt')), \
                        patch.object(rng_reference.hashlib, 'sha256') as digest, \
                        patch.object(rng_reference.OwnedProcess, 'launch', return_value=owned), \
                        patch.object(rng_reference, 'WindowsMemory', return_value=memory), \
                        patch.object(rng_reference.time, 'sleep'), \
                        patch.object(rng_reference.time, 'monotonic') as clock:
                    digest.return_value.hexdigest.return_value = adapter.start_sha256
                    clock.side_effect = [0, 0, 11] if scenario == 'timeout' else None
                    clock.return_value = 0
                    if scenario == 'initialized':
                        self.assertEqual(adapter.capture(exe)['seed'], 7)
                        self.assertEqual(memory.find.call_count, 2)
                    else:
                        with self.assertRaises(TimeoutError if scenario == 'timeout' else RuntimeError):
                            adapter.capture(exe)
                    owned.kill_tree.assert_called_once_with()
                    self.assertEqual(list(root.glob('*.conf')), [])

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
        memory[base - 256] = 0xcd
        memory[base + 0xaf81:base + 0xaf83] = struct.pack('<H', 0x1c7d)
        self.assertEqual(adapter.seed_candidates(bytes(memory), 0, read), [])
        memory[base + 0xaf81:base + 0xaf83] = struct.pack('<H', 0x1c7c)
        unstable = iter([struct.pack('<I', 1), struct.pack('<I', 2)])
        def changing_read(address, size):
            return next(unstable) if address == base + 0x10be8 else read(address, size)
        self.assertEqual(adapter.seed_candidates(bytes(memory), 0, changing_read), [])


if __name__ == '__main__':
    unittest.main()
