#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Bounded single-comparison preview controller; unpaired/inconclusive slice.

Runs one engine JSONL harness session beside one reference process, optionally
mirrors mapped input to an owned SDL window, and never claims parity. See README.md
for the version-1 session configuration and its prerequisites.
"""
import ctypes
import base64
import json
import math
import os
from pathlib import Path, PurePosixPath
import queue
import shutil
import signal
import stat
import subprocess
import sys
import threading
import time
import uuid
from reference_input import ReferenceInputError, WindowsMessages, key_sequence, validate_config
from compare_common import (
    SCHEMA_VERSION, DEFAULT_TIMEOUT, MIN_TIMEOUT, MAX_TIMEOUT, DEFAULT_CHECKPOINT,
    ENGINE_LINE_LIMIT, ENGINE_OUTPUT_LIMIT, REFERENCE_OUTPUT_LIMIT, EVENT_QUEUE_LIMIT,
    OPERATOR_LINE_LIMIT, OPERATOR_QUEUE_LIMIT, ENGINE_QUIT_DRAIN_SECONDS,
    EVENT_WAIT_SLICE, CHUNK, COMMANDS, ControlledError, FloodError)
from compare_config import (
    _reject, _number_field, _exact_schema_version, _argv_field, _table_field,
    _absolute_dir, _contains, _outside_repository, parse_config)
from compare_lock import SessionLock
from compare_process import (
    _thread_ids_of, _job_process_count, _WSL_STEMS, _WSL_DISTRO_PREFIXES,
    is_wsl_launcher_name, _reject_wsl, _suspended_image_path, _abort_windows_launch,
    OwnedProcess)
from compare_wsl import WslEngine
from compare_stream import (
    ConsoleInput, _BoundedDrain, _EventQueue, EngineStdoutReader, _wait_bounded)
from compare_session import (
    _render_ready, _render_text, _offer_menu, _render_complete, Session)
from compare_script import ScriptInput

if os.name == "nt":
    import ctypes.wintypes
    import msvcrt
    from compare_process import (
        CREATE_SUSPENDED, THREAD_SUSPEND_RESUME, JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE,
        JOB_OBJECT_EXTENDED_LIMIT_INFORMATION_CLASS, _IO_COUNTERS, _BASIC_LIMIT_INFO,
        _EXTENDED_LIMIT_INFO, _THREADENTRY32, _PROCESS_ID_LIST, _KERNEL32,
        TH32CS_SNAPTHREAD, JOB_OBJECT_BASIC_PROCESS_ID_LIST_CLASS, THREAD_STILL_ACTIVE,
        WAS_SUSPENDED_ERROR, INVALID_HANDLE_VALUE)
else:
    import fcntl

USAGE = "usage: compare_game.py <session-config.json> [--script <scenario.json>]"


def run_session(config, console=None):
    lock = SessionLock(config["privateRoot"]).acquire()
    engine = reference = None
    stdout_reader = stderr_reader = None
    ref_drains = []
    session = None
    deadline = time.monotonic() + config["timeout"]
    try:
        if config["engine"].get("backend") == "wsl":
            engine = WslEngine.launch(config["engine"], config["timeout"])
        else:
            engine = OwnedProcess.launch(config["engine"]["argv"], config["engine"]["cwd"])
        engine_queue = _EventQueue()
        stdout_reader = EngineStdoutReader(engine.process.stdout, engine_queue,
            engine.filter_event if isinstance(engine, WslEngine) else None)
        engine_queue.reader = stdout_reader
        stdout_reader.start()
        stderr_reader = _BoundedDrain(engine.process.stderr, "engine stderr",
                                      REFERENCE_OUTPUT_LIMIT)
        stderr_reader.start()
        reference_env = None
        if config["reference"].get("input") is not None:
            reference_env = dict(os.environ, SDL_VIDEODRIVER="windib")
        reference = OwnedProcess.launch(config["reference"]["argv"],
                                        config["reference"]["cwd"], env=reference_env)
        ref_drains = [_BoundedDrain(reference.process.stdout, "reference stdout",
                                    REFERENCE_OUTPUT_LIMIT),
                      _BoundedDrain(reference.process.stderr, "reference stderr",
                                    REFERENCE_OUTPUT_LIMIT)]
        for drain in ref_drains:
            drain.start()
        if console is None:
            console = ConsoleInput()
        console.start()
        session = Session(config, engine, reference, engine_queue, console, deadline)
        session.drain_watch = [stderr_reader, *ref_drains]
        code = session.run()
        script_note = console.finish() if isinstance(console, ScriptInput) else None
        _wait_bounded(stdout_reader)
        if engine_queue.failure is not None:
            raise engine_queue.failure
        if console.failure is not None:
            raise console.failure
        if code != 0:
            raise ControlledError(f"session returned an unexpected status {code!r}")
        reference_exit = reference.process.poll()
        if reference_exit is not None:  # same policy as Session._check_reference
            raise ControlledError(
                f"reference exited with code {reference_exit}; session inconclusive "
                "(no comparison result)")
        for drain in ref_drains + [stderr_reader]:
            if drain.failure is not None:
                raise drain.failure
        notes = session.end_notes
        if script_note:
            notes.append(script_note)
        return code, notes
    finally:
        primary = sys.exc_info()[1]
        cleanup_errors = []
        interrupted = None
        # A second Ctrl-C must not skip the other tree or the lock release: each
        # step records any BaseException, an interrupted kill is retried once.
        for owned in (engine, reference):
            if owned is None:
                continue
            finished = False
            for attempt in (1, 2):
                try:
                    owned.kill_tree()
                    finished = True
                    break
                except KeyboardInterrupt as error:
                    interrupted = interrupted or error
                except Exception as error:
                    cleanup_errors.append(str(error))
                    finished = True
                    break
            if not finished:  # Ownership is ambiguous: keep the lock held.
                cleanup_errors.append("cleanup was interrupted twice; owned members may still be alive")
        for waiter in (stdout_reader, stderr_reader, *ref_drains):
            try:
                _wait_bounded(waiter)
            except KeyboardInterrupt as error:
                interrupted = interrupted or error
        # Repeated programmatic runs must release drained pipe handles. Never
        # close a buffered reader while another thread could hold its lock.
        output_pipes = []
        if engine is not None:
            output_pipes.extend([(engine.process.stdout, stdout_reader), (engine.process.stderr, stderr_reader)])
        if reference is not None:
            output_pipes.extend((pipe, ref_drains[index] if index < len(ref_drains) else None)
                                for index, pipe in enumerate((reference.process.stdout, reference.process.stderr)))
        for pipe, reader in output_pipes:
            if pipe is None or (reader is not None and reader.is_alive()):
                continue
            try:
                pipe.close()
            except KeyboardInterrupt as error:
                interrupted = interrupted or error
            except Exception as error:
                cleanup_errors.append(str(error))
        if not cleanup_errors:
            lock.release()  # A failed kill leaves ownership ambiguous: keep the lock until exit.
        if cleanup_errors:
            detail = "cleanup failed: " + "; ".join(cleanup_errors)
            if primary is None:
                raise ControlledError(detail)
            if isinstance(primary, ControlledError):
                raise ControlledError(f"{primary}; {detail}") from primary
            print(f"compare_game: {detail}", file=sys.stderr)
        if interrupted is not None and primary is None:
            raise interrupted


def _announce(notes):
    for note in notes:
        print(note)


def _raise_interrupt(signum, frame):
    raise KeyboardInterrupt


def _install_termination_handlers():
    """Turn SIGTERM/SIGHUP (POSIX) and SIGBREAK (Windows) into the Ctrl-C path so
    run_session's finally kills the owned trees; returns the previous handlers."""
    names = ("SIGBREAK",) if os.name == "nt" else ("SIGTERM", "SIGHUP")
    previous = {}
    for name in names:
        number = getattr(signal, name, None)
        if number is not None:
            previous[number] = signal.signal(number, _raise_interrupt)
    return previous


def main(argv):
    if len(argv) not in (2, 4) or (len(argv) == 4 and argv[2] != '--script'):
        print(USAGE, file=sys.stderr)
        return 2
    try:
        _install_termination_handlers()
        config = parse_config(Path(argv[1]))
        scripted = ScriptInput.from_file(argv[3]) if len(argv) == 4 else None
        code, notes = run_session(config, scripted) if scripted is not None else run_session(config)
        _announce(notes)
        return code
    except ControlledError as error:
        print(f"compare_game: {error}", file=sys.stderr)
        return 1
    except ReferenceInputError as error:
        print(f"compare_game: {error}", file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        print("compare_game: cancelled by operator", file=sys.stderr)
        return 1
    except Exception as error:  # fail closed rather than claim success
        print(f"compare_game: unexpected failure: {error!r}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
