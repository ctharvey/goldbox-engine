#!/usr/bin/env bash
# Serialized, low-parallelism build (and optional tests) for WSL.
# Why: overlapping `-j4` builds plus unbounded piped runs exhausted Windows
# commit space on 2026-10-12. flock = one build at a time; -j2 = bounded
# memory; timeout = a hung step cannot run forever.
# Usage: scripts/wsl-build.sh [ctest args, e.g. -R "ecl_vm|exploration"]
set -euo pipefail

cd "$(dirname "$0")/.."

LOCK=/tmp/goldbox-build.lock
exec 9>"$LOCK"
flock -w 600 9 || { echo "Another goldbox build holds $LOCK" >&2; exit 1; }

timeout 900 cmake --build build -j2
if [ "$#" -gt 0 ]; then
    timeout 600 ctest --test-dir build --output-on-failure -j2 "$@"
fi
