# SPDX-License-Identifier: GPL-3.0-or-later
"""Synthetic checks for the live adapter's comparison and checkpoint boundary."""
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from pool_startup_reference import matches, movement_flags, read_checkpoint, verify


class CheckpointTest(unittest.TestCase):
    def test_movement_flags_use_the_separate_word_buffer(self):
        data = bytearray(65536)
        data[0x49d6:0x49da] = bytes([0x20,0,0,0x30])
        reads = []

        def read(address, size):
            reads.append((address, size))
            return bytes([1,2])

        self.assertEqual(movement_flags(data,read,0x10000000), {"28105": 513,"28117": 513})
        self.assertEqual(reads, [(0x10030020 + 2*(0x6dc9-0x6b00),2),
                                (0x10030020 + 2*(0x6dd5-0x6b00),2)])
        with self.assertRaises(RuntimeError):
            movement_flags(data,lambda address,size: b"\0",0)
        data[0x49d6:0x49da] = bytes([0xff,0xff,0,0x30])
        with self.assertRaises(ValueError):
            movement_flags(data,read,0)
        data[0x49d6:0x49da] = bytes([0,0,0xff,0xff])
        with self.assertRaises(ValueError):
            movement_flags(data,read,0)
        with self.assertRaises(ValueError):
            movement_flags(data[:100],read,0)
        reads.clear()
        self.assertEqual(movement_flags(bytes(65536),read,0), {})
        self.assertEqual(reads, [])

    def test_oversized_reference_is_rejected_before_reading(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "oversized.exe"
            with path.open("wb") as stream:
                stream.truncate(8 * 1024 * 1024 + 1)
            with patch.object(Path, "read_bytes", side_effect=AssertionError("Unbounded resource read")):
                with self.assertRaises(ValueError):
                    verify(path, "0" * 64)

    def setUp(self):
        self.expected = dict(status="original-resource-replay", column=2, row=3, facing=1,
                             bytecodeOffset=25, codeBase=0x9900, programRecord=0,
                             flags={"18967": 1, "19140": 0, "19141": 1},
                             saveRoundTrip=True, deterministicReplay=True)
        self.observed = {key: self.expected[key] for key in ("column", "row", "facing", "flags")}
        self.observed["programCounter"] = 0x991a

    def test_divergence_is_a_failure(self):
        self.assertTrue(matches(self.expected, self.observed))
        for key in ("column", "row", "facing", "programCounter"):
            with self.subTest(key=key):
                altered = dict(self.observed)
                altered[key] += 1
                self.assertFalse(matches(self.expected, altered))
        altered = dict(self.observed, flags=dict(self.observed["flags"], **{"19141": 0}))
        self.assertFalse(matches(self.expected, altered))

    def test_only_completed_bounded_engine_checks_are_accepted(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "checkpoint.json"
            path.write_text(json.dumps(self.expected))
            self.assertEqual(read_checkpoint(path), self.expected)
            for key, value in [("row", 16), ("column", True), ("facing", -1), ("codeBase", 65535),
                               ("saveRoundTrip", False), ("deterministicReplay", False), ("programRecord", 1),
                               ("programRecord", False),
                               ("flags", {"19141": 1}), ("flags", {"18967": 1, "19140": 0, "19141": True})]:
                with self.subTest(key=key, value=value):
                    path.write_text(json.dumps(dict(self.expected, **{key: value})))
                    with self.assertRaises(ValueError):
                        read_checkpoint(path)


if __name__ == "__main__":
    unittest.main()
