#!/usr/bin/env python3
"""Basic source- and asset-contamination guard. Not a legal/license audit."""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
IGNORE_DIRS = {".git", ".idea", ".vscode", "build", "build-release", "node_modules", "dist", "out", "__pycache__"}
FORBIDDEN_DIRS = {"originals", "game-data", "proprietary", "ssi-assets"}
FORBIDDEN_SUFFIXES = {".dsk", ".img", ".iso", ".exe", ".dax", ".sav", ".pak"}
SOURCE_SUFFIXES = {".c", ".cpp", ".h", ".hpp", ".ts"}
GPL_MARKERS = ("gnu general public license", "spdx-license-identifier: gpl", "spdx-license-identifier: agpl")

violations = []
for path in ROOT.rglob("*"):
    if not path.is_file():
        continue
    rel = path.relative_to(ROOT)
    if any(part in IGNORE_DIRS or part.startswith("build-") for part in rel.parts):
        continue
    if any(part in FORBIDDEN_DIRS for part in rel.parts):
        violations.append(f"restricted asset directory: {rel}")
    if path.suffix.lower() in FORBIDDEN_SUFFIXES:
        violations.append(f"likely proprietary/binary asset: {rel}")
    if path.suffix.lower() in SOURCE_SUFFIXES:
        txt = path.read_text(encoding="utf-8", errors="replace").lower()
        if any(marker in txt for marker in GPL_MARKERS):
            violations.append(f"license marker requires review: {rel}")

if violations:
    print("Repository hygiene FAILED:\n" + "\n".join("- " + v for v in violations))
    sys.exit(1)
print("Repository hygiene: PASS (heuristic; still review all third-party code manually)")
