# SPDX-License-Identifier: GPL-3.0-or-later
"""Synthetic checks for the live adapter's comparison and checkpoint boundary."""
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from pool_startup_reference import matches, read_checkpoint, verify


class CheckpointTest(unittest.TestCase):
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
