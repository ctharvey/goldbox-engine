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
    import signal

USAGE = "usage: compare_game.py <session-config.json>"


def run_session(config):
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
        console = ConsoleInput()
        console.start()
        session = Session(config, engine, reference, engine_queue, console, deadline)
        session.drain_watch = [stderr_reader, *ref_drains]
        code = session.run()
        _wait_bounded(stdout_reader)
        if engine_queue.failure is not None:
            raise engine_queue.failure
        if console.failure is not None:
            raise console.failure
        if code != 0:
            raise ControlledError(f"session returned an unexpected status {code!r}")
        reference_exit = reference.process.poll()
        if reference_exit is not None and reference_exit != 0:
            raise ControlledError(
                f"reference exited with code {reference_exit}; session inconclusive "
                "(no comparison result)")
        for drain in ref_drains + [stderr_reader]:
            if drain.failure is not None:
                raise drain.failure
        notes = session.end_notes
        return code, notes
    finally:
        cleanup_errors = []
        for owned in (engine, reference):
            if owned is not None:
                try:
                    owned.kill_tree()
                except Exception as error:
                    cleanup_errors.append(str(error))
        _wait_bounded(stdout_reader)
        _wait_bounded(stderr_reader)
        for drain in ref_drains:
            _wait_bounded(drain)
        lock.release()
        if cleanup_errors:
            raise ControlledError("cleanup failed: " + "; ".join(cleanup_errors))


def _announce(notes):
    for note in notes:
        print(note)


def main(argv):
    if len(argv) != 2:
        print(USAGE, file=sys.stderr)
        return 2
    try:
        config = parse_config(Path(argv[1]))
        code, notes = run_session(config)
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
