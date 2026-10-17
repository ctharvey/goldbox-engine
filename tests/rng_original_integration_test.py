#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Opt-in native Windows regression: original-code matrix, live seed, replay.

Private resources are configured explicitly through environment variables.
Exit 77 means unavailable, never a parity pass. No GUI interaction is needed.
"""
import json
import os
from pathlib import Path
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
from rng_debug import compare
from rng_reference import PoolDosRandom


def main():
    image = os.environ.get('GOLDBOX_RNG_IMAGE')
    dosbox = os.environ.get('GOLDBOX_RNG_DOSBOX')
    if not image and not dosbox:
        print('SKIP: native Windows plus GOLDBOX_RNG_IMAGE and GOLDBOX_RNG_DOSBOX required')
        return 77
    if not image or not dosbox:
        raise ValueError('partial configuration: define both GOLDBOX_RNG_IMAGE and GOLDBOX_RNG_DOSBOX')
    if os.name != 'nt':
        print('SKIP: live DOSBox capture requires native Windows')
        return 77
    if len(sys.argv) != 2:
        raise ValueError('usage: rng_original_integration_test.py <goldbox_rng_probe>')
    adapter = PoolDosRandom(image, os.environ.get('GOLDBOX_RNG_UNICORN_PATH'))
    deadline = time.monotonic() + 75
    checked = 0

    def verify(seed, bound, count):
        nonlocal checked
        if time.monotonic() >= deadline:
            raise TimeoutError('local original RNG regression exceeded its total budget')
        report = compare(adapter, sys.argv[1], seed, bound, count)
        if not report['match']:
            mismatch = next(row for row in report['rows'] if not row['match'])
            raise AssertionError(f'RNG mismatch seed={seed:#010x}, bound={bound}: {mismatch}')
        checked += count

    for seed in [0, 1, 0xffffffff, 0x12345678, 0x2e4d0932]:
        for bound in [0, 1, 2, 6, 20, 256, 65535]:
            verify(seed, bound, 8)
    snapshot = adapter.capture(dosbox)  # Owned reference cleanup completes before return.
    for bound in [0, 1, 20, 256, 65535]:
        verify(snapshot['seed'], bound, 32)
    print(json.dumps(dict(status='PASS', original_matrix_draws=280,
                          captured_seed_draws=160, total_draws=checked,
                          captured_seed=f"0x{snapshot['seed']:08X}",
                          scope='live seed capture plus isolated original-code/C++ replay')))
    return 0


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except Exception as error:
        print(f'original RNG regression: {error}', file=sys.stderr)
        raise SystemExit(1)
