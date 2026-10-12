#!/usr/bin/env python3
"""Validate third-party provenance records and adopted-code markers (offline, stdlib only)."""
from __future__ import annotations

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REGISTRY = ROOT / "attribution" / "sources.json"
SOURCE_SUFFIXES = {".cpp", ".c", ".h", ".hpp", ".ts", ".py"}
SKIP_DIRS = {".git", "build", "node_modules", "dist", "__pycache__"}
KINDS = {"adapted", "dependency", "reference"}
SHA40 = re.compile(r"^[0-9a-f]{40}$")
ID = re.compile(r"^[a-z0-9]+(?:-[a-z0-9]+)*$")
MARKER = re.compile(r"Attribution-ID:\s*([a-z0-9-]+)")
errors: list[str] = []


def require(condition: bool, message: str) -> None:
    if not condition:
        errors.append(message)


def safe_file(value: str, label: str) -> Path | None:
    if not isinstance(value, str) or not value:
        errors.append(f"{label}: expected nonempty relative file path")
        return None
    rel = Path(value)
    if rel.is_absolute() or ".." in rel.parts or rel.as_posix() != value:
        errors.append(f"{label}: unsafe or noncanonical path: {value!r}")
        return None
    full = ROOT / rel
    if not full.is_file() or full.is_symlink():
        errors.append(f"{label}: missing or symlinked file: {value}")
        return None
    return full


try:
    inventory = json.loads(REGISTRY.read_text(encoding="utf-8"))
except (OSError, ValueError) as exc:
    print(f"Attribution check FAILED: cannot parse {REGISTRY}: {exc}")
    sys.exit(1)

require(isinstance(inventory, dict), "registry must contain a JSON object")
if not isinstance(inventory, dict):
    sys.exit(1)
require(inventory.get("schemaVersion") == 1, "registry schemaVersion must be 1")
entries = inventory.get("entries")
require(isinstance(entries, list) and bool(entries), "entries must be a nonempty array")
if not isinstance(entries, list):
    sys.exit(1)

seen_ids: set[str] = set()
adopted_files: dict[str, str] = {}
all_registry_files: set[str] = set()
for index, entry in enumerate(entries):
    label = f"entries[{index}]"
    if not isinstance(entry, dict):
        errors.append(f"{label}: must be an object")
        continue
    source_id = entry.get("id")
    if not isinstance(source_id, str) or not ID.fullmatch(source_id):
        errors.append(f"{label}: invalid id")
        continue
    require(source_id not in seen_ids, f"{source_id}: duplicated id")
    seen_ids.add(source_id)
    kind = entry.get("kind")
    require(kind in KINDS, f"{source_id}: invalid kind")
    for key in ("project", "upstreamRepository", "license", "licenseUrl",
                "copyrightNotice", "purpose", "modifications", "reviewed"):
        value = entry.get(key)
        require(isinstance(value, str) and bool(value.strip()), f"{source_id}: missing {key}")
    upstream = entry.get("upstreamRepository")
    require(isinstance(upstream, str) and upstream.startswith("https://github.com/"),
            f"{source_id}: upstreamRepository must be a GitHub HTTPS URL")
    version = entry.get("upstreamRevision")
    if kind in ("adapted", "reference"):
        require(isinstance(version, str) and bool(SHA40.fullmatch(version)),
                f"{source_id}: need immutable 40-character upstream SHA")
    elif version is not None:
        require(isinstance(version, str) and bool(SHA40.fullmatch(version)),
                f"{source_id}: revision must be SHA or null")
    upstream_files = entry.get("upstreamFiles")
    require(isinstance(upstream_files, list), f"{source_id}: upstreamFiles must be an array")
    if kind == "adapted":
        require(isinstance(upstream_files, list) and bool(upstream_files),
                f"{source_id}: adopted code must cite exact upstream files")
    if isinstance(upstream_files, list):
        require(len(upstream_files) == len(set(map(str, upstream_files))),
                f"{source_id}: duplicate upstream path")
        for source in upstream_files:
            require(isinstance(source, str) and bool(source) and
                    not source.startswith("/") and ".." not in Path(source).parts,
                    f"{source_id}: invalid upstream file path {source!r}")
    local_files = entry.get("localFiles")
    require(isinstance(local_files, list), f"{source_id}: localFiles must be an array")
    if kind in ("adapted", "dependency"):
        require(isinstance(local_files, list) and bool(local_files),
                f"{source_id}: kind={kind} must identify local integration files")
    if kind == "reference":
        require(local_files == [], f"{source_id}: research-only reference must not claim incorporated files")
    if isinstance(local_files, list):
        require(len(local_files) == len(set(map(str, local_files))),
                f"{source_id}: duplicate local file")
        for local in local_files:
            full = safe_file(local, source_id)
            if full is None:
                continue
            all_registry_files.add(local)
            if kind == "adapted":
                if local in adopted_files:
                    errors.append(f"{source_id}: {local} also claimed by {adopted_files[local]}")
                adopted_files[local] = source_id
                header = "\n".join(full.read_text(encoding="utf-8", errors="replace").splitlines()[:25])
                require(f"Attribution-ID: {source_id}" in header,
                        f"{source_id}: missing local attribution ID header in {local}")
                require("SPDX-License-Identifier:" in header,
                        f"{source_id}: missing SPDX license identifier in {local}")
                notice = entry.get("copyrightNotice")
                require(isinstance(notice, str) and notice in header,
                        f"{source_id}: missing upstream copyright notice in {local}")
    license_path = entry.get("licenseFile")
    if kind == "adapted":
        require(isinstance(license_path, str), f"{source_id}: adapted code must preserve a local license")
    if license_path is not None:
        license_file = safe_file(license_path, source_id + " license")
        if license_file is not None and kind == "adapted" and entry.get("license") == "MIT":
            lic = license_file.read_text(encoding="utf-8")
            require("MIT License" in lic and entry.get("copyrightNotice", "") in lic,
                    f"{source_id}: preserved upstream MIT license/copyright does not match")

# Detect local source explicitly attributed to a project but left unregistered.
for path in ROOT.rglob("*"):
    if not path.is_file() or path.suffix.lower() not in SOURCE_SUFFIXES:
        continue
    rel = path.relative_to(ROOT).as_posix()
    if any(part in SKIP_DIRS or part.startswith("build-") for part in path.relative_to(ROOT).parts):
        continue
    header = "\n".join(path.read_text(encoding="utf-8", errors="replace").splitlines()[:25])
    markers = MARKER.findall(header)
    for source_id in markers:
        require(source_id in seen_ids, f"{rel}: undeclared Attribution-ID {source_id}")
        require(adopted_files.get(rel) == source_id or rel in all_registry_files,
                f"{rel}: marker not linked to localFiles in registry")
    if re.search(r"\badapted\b.*\bfrom\b|\badapted\b.*\bwith attribution\b", header, re.I):
        require(bool(markers), f"{rel}: upstream-adapted source has no Attribution-ID header")

if errors:
    print("Attribution check FAILED:\n" + "\n".join(f"- {e}" for e in errors))
    sys.exit(1)
print(f"Attribution check: PASS ({len(entries)} records, {len(adopted_files)} adopted local files; local check only)")
