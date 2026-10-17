#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Seeded original-runtime versus C++ engine RNG debug screen (private inputs)."""
import argparse
import json
import os
from pathlib import Path
import queue
import subprocess
import sys
import threading

from rng_reference import PoolDosRandom


def integer(text, maximum):
    value = int(text, 16 if text.lower().startswith('0x') else 10)
    if not 0 <= value <= maximum:
        raise ValueError(f'value must be 0..{maximum}')
    return value


def engine_rolls(executable, adapter, seed, bound, count):
    path = Path(executable).resolve(strict=True)
    command = [str(path)]
    if os.name == 'nt' and path.suffix.lower() != '.exe':
        if path.drive.lower() != 'c:':
            raise ValueError('WSL probe currently requires a C: workspace path')
        linux = '/mnt/c/' + path.as_posix()[3:]
        command = ['wsl.exe', '-d', 'Ubuntu', '--exec', 'timeout', '10', linux]
    command.extend(str(value) for value in (seed, bound, count, adapter.multiplier, adapter.increment))
    result = subprocess.run(command, capture_output=True, text=True, timeout=15, check=True)
    return json.loads(result.stdout)


def compare(adapter, executable, seed, bound, count):
    if type(seed) is not int or not 0 <= seed <= 0xffffffff:
        raise ValueError('seed must be an unsigned 32-bit integer')
    if type(bound) is not int or not 0 <= bound <= 65535:
        raise ValueError('runtime bound must be 0..65535')
    if type(count) is not int or not 1 <= count <= 64:
        raise ValueError('draw count must be 1..64')
    old = adapter.rolls(seed, bound, count)
    new = engine_rolls(executable, adapter, seed, bound, count)
    if len(old) != count or len(new) != count:
        raise ValueError('incomplete roll sequence; no comparison result')
    rows = []
    for index, (left, right) in enumerate(zip(old, new), 1):
        for row in (left, right):
            if (not isinstance(row, dict) or row.get('index') != index
                    or any(type(row.get(key)) is not int or not 0 <= row[key] <= limit
                           for key, limit in [('before', 0xffffffff), ('after', 0xffffffff), ('value', 65535)])):
                raise ValueError('invalid roll sequence; no comparison result')
        rows.append(dict(index=index, old=left, new=right, match=left == right))
    return dict(seed=seed, bound=bound, count=count, match=all(row['match'] for row in rows), rows=rows,
                reference='original DOS runtime in isolated emulator; not live game draw tracing')


def screen(args, adapter):
    import tkinter as tk
    from tkinter import ttk
    root = tk.Tk()
    root.title('Goldbox - seeded RNG comparison')
    root.geometry('1050x640')
    panel = ttk.Frame(root, padding=18)
    panel.pack(fill='both', expand=True)
    ttk.Label(panel, text='Original DOS vs Goldbox Engine', font=('Segoe UI', 19, 'bold')).pack(anchor='w')
    ttk.Label(panel, text='Same seed, same runtime bound, same draw count. Compare values AND internal state.').pack(anchor='w', pady=5)
    ttk.Label(panel, text='Original side executes original code in isolation. Live capture reads a seed only; it does not drive game rolls.').pack(anchor='w')
    inputs = ttk.Frame(panel)
    inputs.pack(fill='x', pady=16)
    entries = []
    seed = tk.StringVar(value=args.seed)
    bound = tk.StringVar(value=str(args.bound))
    count = tk.StringVar(value=str(args.count))
    for name, variable, width in [('Seed (decimal / 0x hex)', seed, 20), ('Runtime bound', bound, 10), ('Draws (1-64)', count, 8)]:
        ttk.Label(inputs, text=name).pack(side='left', padx=(0, 6))
        entry = ttk.Entry(inputs, textvariable=variable, width=width)
        entry.pack(side='left', padx=(0, 18))
        entries.append(entry)
    source = tk.StringVar(value='Seed source: manual. Runtime Random(n) returns 0..n-1; n=0 still advances state.')
    ttk.Label(panel, textvariable=source, wraplength=1000).pack(anchor='w')
    status = tk.StringVar(value='Ready')
    ttk.Label(panel, textvariable=status, font=('Segoe UI', 12, 'bold'), wraplength=1000).pack(anchor='w', pady=12)
    columns = ('draw', 'old_value', 'new_value', 'old_before', 'new_before', 'old_after', 'new_after', 'match')
    table = ttk.Treeview(panel, columns=columns, show='headings', height=16)
    for key, label in zip(columns, ['Draw', 'DOS value', 'Engine value', 'DOS state before', 'Engine state before', 'DOS state after', 'Engine state after', 'Result']):
        table.heading(key, text=label)
        table.column(key, width=65 if key in ('draw', 'match') else 130, anchor='center')
    table.tag_configure('bad', background='#ffd4d4')
    table.pack(fill='both', expand=True)
    scrollbar = ttk.Scrollbar(panel, orient='vertical', command=table.yview)
    table.configure(yscrollcommand=scrollbar.set)
    scrollbar.pack(side='right', fill='y')
    buttons = ttk.Frame(panel)
    buttons.pack(fill='x', pady=(12, 0))
    messages = queue.Queue()
    worker = None
    closing = False

    def changed(seed_changed=False):
        table.delete(*table.get_children())
        status.set('Inputs changed. Compare again to obtain a result.')
        if seed_changed:
            source.set('Seed source: manual. Runtime bound is exclusive; zero still advances state.')
    seed.trace_add('write', lambda *unused: changed(True))
    bound.trace_add('write', lambda *unused: changed())
    count.trace_add('write', lambda *unused: changed())

    def start(job):
        nonlocal worker
        if worker is not None:
            return
        table.delete(*table.get_children())
        status.set('Working within fixed time and instruction limits...')
        run_button.state(['disabled'])
        capture_button.state(['disabled'])
        for entry in entries:
            entry.state(['disabled'])
        def work():
            try:
                messages.put(('ok', job()))
            except Exception as error:
                messages.put(('error', str(error)))
        worker = threading.Thread(target=work, daemon=False)
        worker.start()

    def run():
        try:
            values = integer(seed.get(), 0xffffffff), integer(bound.get(), 65535), integer(count.get(), 64)
        except ValueError as error:
            status.set(str(error))
            return
        start(lambda: ('comparison', compare(adapter, args.engine, *values)))

    def capture():
        start(lambda: ('capture', adapter.capture(args.dosbox)))

    def close():
        nonlocal closing
        closing = True
        status.set('Closing after bounded worker and owned DOSBox cleanup...')
        if worker is None:
            root.destroy()

    def poll():
        nonlocal worker
        if worker is not None and not worker.is_alive():
            worker.join()
            worker = None
            kind, result = messages.get_nowait()
            run_button.state(['!disabled'])
            for entry in entries:
                entry.state(['!disabled'])
            if args.dosbox:
                capture_button.state(['!disabled'])
            if kind == 'error':
                status.set('ERROR: ' + result)
                print('rng debug: ' + result, file=sys.stderr, flush=True)
            elif result[0] == 'capture':
                snapshot = result[1]
                seed.set(f"0x{snapshot['seed']:08X}")
                source.set(f"Seed source: live owned DOSBox PID {snapshot['pid']}, read-only snapshot. Reference process cleaned up.")
                status.set('Captured. Replaying this seed on both sides...')
                root.after(100, run)
            else:
                report = result[1]
                status.set(f"{'MATCH' if report['match'] else 'MISMATCH'}: {report['count']} draws; values and before/after states compared.")
                print(json.dumps(dict(seed=report['seed'], bound=report['bound'], count=report['count'],
                                      match=report['match'], seed_source=source.get())), flush=True)
                for row in report['rows']:
                    left, right = row['old'], row['new']
                    table.insert('', 'end', values=(row['index'], left['value'], right['value'],
                        f"{left['before']:08X}", f"{right['before']:08X}", f"{left['after']:08X}",
                        f"{right['after']:08X}", 'PASS' if row['match'] else 'FAIL'),
                        tags=() if row['match'] else ('bad',))
        if closing and worker is None:
            root.destroy()
        else:
            root.after(100, poll)

    run_button = ttk.Button(buttons, text='Compare rolls', command=run)
    run_button.pack(side='left')
    capture_button = ttk.Button(buttons, text='Capture seed from private DOSBox', command=capture)
    capture_button.pack(side='left', padx=12)
    if not args.dosbox:
        capture_button.state(['disabled'])
    ttk.Label(buttons, text='Debug window closes after 5 minutes. Gameplay RNG is not wired yet.').pack(side='right')
    root.protocol('WM_DELETE_WINDOW', close)
    root.after(300000, close)
    root.after(100, poll)
    root.after(250, capture if args.dosbox else run)
    root.mainloop()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--engine', required=True, help='built goldbox_rng_probe executable')
    parser.add_argument('--image', required=True, help='private fingerprinted unpacked DOS load image')
    parser.add_argument('--unicorn-path', help='optional private Python dependency directory')
    parser.add_argument('--dosbox', help='harness private DOSBox.exe beside GAME/POOLRAD copy')
    parser.add_argument('--seed', default='0x12345678')
    parser.add_argument('--bound', type=int, default=20)
    parser.add_argument('--count', type=int, default=16)
    parser.add_argument('--check', action='store_true', help='emit comparison JSON instead of opening the screen')
    args = parser.parse_args()
    try:
        adapter = PoolDosRandom(args.image, args.unicorn_path)
        if args.check:
            result = compare(adapter, args.engine, integer(args.seed, 0xffffffff), args.bound, args.count)
            print(json.dumps(result))
            return 0 if result['match'] else 1
        screen(args, adapter)
        return 0
    except Exception as error:
        print(f'rng debug: {error}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
