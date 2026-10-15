#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Read-only inventory; emits metadata and decoder diagnostics, never payloads."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys


def family(path):
    if path.suffix.upper() not in {".DAX", ".GLB"}:
        return "other"
    match = re.fullmatch(r"([A-Z]+)\d*", path.stem.upper())
    name = match.group(1) if match else "unknown"
    return f"{path.suffix[1:].lower()}:{name}"


def fingerprint(path):
    before = path.stat()
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    after = path.stat()
    if (before.st_size, before.st_mtime_ns) != (after.st_size, after.st_mtime_ns):
        raise ValueError("file changed during fingerprinting")
    return after.st_size, digest.hexdigest()


def decode(path, cli, profile):
    if path.suffix.upper() == ".GLB":
        return [{"decoder": "glb", "status": "unsupported"}]
    if path.suffix.upper() != ".DAX":
        return []
    modes = ["dax"]
    resource = family(path).split(":")[-1]
    if resource == "GEO":
        modes.append("geo")
    if resource == "ECL":
        modes.append("ecl")
    results = []
    for mode in modes:
        if mode == "ecl" and profile is None:
            results.append({"decoder": mode, "status": "unsupported", "diagnostic": "ECL requires a profile"})
            continue
        command = [str(cli), f"--scan-{mode}", str(path)]
        if profile is not None:
            command.extend(["--game-profile", str(profile)])
        try:
            process = subprocess.run(command, capture_output=True, text=True, timeout=30, check=False)
            # Our scan modes output only counts, sizes, record IDs and errors.
            results.append({"decoder": mode, "status": "passed" if process.returncode == 0 else "failed",
                            "diagnostic": (process.stdout + process.stderr).strip()})
        except (OSError, subprocess.TimeoutExpired) as error:
            results.append({"decoder": mode, "status": "failed", "diagnostic": type(error).__name__})
    return results


def scan(installation, cli, profile=None):
    installation = Path(installation).absolute()
    if installation.is_symlink() or installation.resolve() != installation or not installation.is_dir():
        raise ValueError("installation must be a non-symlinked directory")
    files = []
    traversal_errors = []
    def traversal_error(error):
        traversal_errors.append({"status": "failed", "diagnostic": type(error).__name__})
    for directory, directories, names in os.walk(installation, followlinks=False, onerror=traversal_error):
        for name in sorted(directories[:]):
            path = Path(directory) / name
            if path.is_symlink() or path.resolve() != path.absolute():
                files.append({"path": path.relative_to(installation).as_posix(), "status": "skipped-link"})
                directories.remove(name)
        directories.sort()
        for name in sorted(names):
            path = Path(directory) / name
            entry = {"path": path.relative_to(installation).as_posix(), "family": family(path)}
            if path.is_symlink() or path.resolve() != path.absolute() or not path.is_file():
                entry["status"] = "skipped-link-or-special"
            else:
                try:
                    size, digest = fingerprint(path)
                    entry.update(size=size, sha256=digest, status="inventoried", decoders=decode(path, cli, profile))
                except (OSError, ValueError) as error:
                    entry.update(status="failed", diagnostic=type(error).__name__)
            files.append(entry)
    files.sort(key=lambda entry: entry["path"])
    failures = len(traversal_errors) + sum(entry["status"] == "failed" or
        any(result["status"] == "failed" for result in entry.get("decoders", [])) for entry in files)
    return {"schemaVersion": 1, "compatibility": "unverified", "files": files,
            "traversalErrors": traversal_errors, "failureCount": failures}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("installation", type=Path)
    parser.add_argument("--cli", type=Path, required=True, help="built goldbox_cli executable")
    parser.add_argument("--profile", type=Path)
    args = parser.parse_args()
    try:
        cli = args.cli.resolve(strict=True)
        profile = args.profile.resolve(strict=True) if args.profile else None
        report = scan(args.installation, cli, profile)
        print(json.dumps(report, indent=2))
        return 1 if report["failureCount"] else 0
    except (OSError, ValueError) as error:
        print(f"Scan failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
