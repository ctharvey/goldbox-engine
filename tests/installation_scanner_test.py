# SPDX-License-Identifier: GPL-3.0-or-later
"""All files are synthetic; temporary resources stay outside the checkout."""
import hashlib
import importlib.util
from pathlib import Path
import struct
import sys
import tempfile
import unittest

SPEC = importlib.util.spec_from_file_location("scanner", Path(__file__).parents[1] / "scripts/scan_installation.py")
SCANNER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(SCANNER)
CLI = Path(sys.argv[1])
PROFILE = Path(sys.argv[2])
sys.argv = sys.argv[:1]


def dax(raw):
    packed = b"".join(bytes([len(raw[i:i + 128]) - 1]) + raw[i:i + 128] for i in range(0, len(raw), 128))
    return struct.pack("<HB IHH", 9, 7, 0, len(raw), len(packed)) + packed


class ScannerTests(unittest.TestCase):
    def test_inventory_and_decoders(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            geo = dax(bytes(1026))
            (root / "geo1.dax").write_bytes(geo)
            (root / "ECL1.DAX").write_bytes(dax(bytes(5)))  # five EXIT entries
            (root / "broken.dax").write_bytes(b"bad")
            (root / "late.GLB").write_bytes(b"synthetic")
            (root / "note.txt").write_text("synthetic", encoding="ascii")
            before = {p.name: p.read_bytes() for p in root.iterdir()}
            report = SCANNER.scan(root, CLI, PROFILE)
            files = {entry["path"]: entry for entry in report["files"]}
            self.assertEqual(files["geo1.dax"]["sha256"], hashlib.sha256(geo).hexdigest())
            self.assertEqual(files["geo1.dax"]["size"], len(geo))
            self.assertEqual([d["status"] for d in files["geo1.dax"]["decoders"]], ["passed", "passed"])
            self.assertEqual([d["status"] for d in files["ECL1.DAX"]["decoders"]], ["passed", "passed"])
            self.assertEqual(files["late.GLB"]["decoders"][0]["status"], "unsupported")
            self.assertEqual(report["failureCount"], 1)
            self.assertEqual(before, {p.name: p.read_bytes() for p in root.iterdir()})
            self.assertEqual(report["compatibility"], "unverified")
            self.assertNotIn("synthetic", str(report))

    def test_links_and_missing_root(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / "data.txt").write_bytes(b"synthetic")
            try:
                (root / "link").symlink_to(root / "data.txt")
            except OSError:
                self.skipTest("host cannot create symlinks")
            report = SCANNER.scan(root, CLI)
            self.assertEqual(report["files"][1]["status"], "skipped-link-or-special")
            with self.assertRaises(ValueError):
                SCANNER.scan(root / "missing", CLI)


if __name__ == "__main__":
    unittest.main()
