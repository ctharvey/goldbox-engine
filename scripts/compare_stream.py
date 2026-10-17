# SPDX-License-Identifier: GPL-3.0-or-later
"""Bounded reader and drain threads for the compare_game controller."""
import json
import os
import queue
import sys
import threading
from compare_common import (CHUNK, ControlledError, ENGINE_LINE_LIMIT, ENGINE_OUTPUT_LIMIT,
                            EVENT_QUEUE_LIMIT, FloodError, OPERATOR_LINE_LIMIT,
                            OPERATOR_QUEUE_LIMIT)


class ConsoleInput(threading.Thread):
    """Bounded operator stdin reader; oversized or flooded input fails the
    session rather than being silently discarded."""

    def __init__(self):
        super().__init__(daemon=True)
        self.queue = queue.Queue(maxsize=OPERATOR_QUEUE_LIMIT)
        self.failure = None

    def run(self):
        try:
            buffer = bytearray()
            while self.failure is None:
                try:
                    # Avoid holding Python's buffered stdin lock at interpreter shutdown.
                    chunk = os.read(sys.stdin.fileno(), 4096)
                except (OSError, ValueError):
                    break
                if not chunk:
                    break
                buffer.extend(chunk)
                while b"\n" in buffer and self.failure is None:
                    line, _, rest = bytes(buffer).partition(b"\n")
                    buffer = bytearray(rest)
                    self._offer(line)
                if len(buffer) > OPERATOR_LINE_LIMIT:
                    self.failure = FloodError("operator input exceeds its bounded line limit")
            if self.failure is None:
                if len(buffer) > OPERATOR_LINE_LIMIT:
                    self.failure = ControlledError(
                        f"operator input exceeds the {OPERATOR_LINE_LIMIT} byte "
                        "line bound")
                elif buffer:
                    self._offer(bytes(buffer))
        except Exception as error:  # defensive: the reader must not exit silently
            self.failure = ControlledError(f"operator console reader failed: {error}")
        try:
            self.queue.put_nowait(None)
        except queue.Full:
            pass

    def _offer(self, raw):
        if len(raw) > OPERATOR_LINE_LIMIT:
            self.failure = ControlledError(
                f"operator line exceeds the {OPERATOR_LINE_LIMIT} byte bound")
            return
        text = raw.decode("utf-8", errors="replace").strip()
        try:
            self.queue.put_nowait(text)
        except queue.Full:
            self.failure = FloodError(
                "operator input queue exceeded its bounded limit")


class _BoundedDrain(threading.Thread):
    """Bounded drain for stderr-style pipes; surfaces flood/error failures."""

    def __init__(self, stream, what, limit):
        super().__init__(daemon=True)
        self.stream = stream
        self.what = what
        self.limit = limit
        self.total = 0
        self.failure = None

    def run(self):
        try:
            while True:
                try:
                    chunk = self.stream.read1(CHUNK)
                except OSError:
                    return
                if not chunk:
                    return
                self.total += len(chunk)
                if self.total > self.limit:
                    self.failure = FloodError(
                        f"{self.what} exceeded the bounded drain limit ({self.limit} bytes)")
                    return
        except Exception as error:  # defensive: the drain must not exit silently
            self.failure = ControlledError(f"{self.what} drain failed: {error}")


class _EventQueue:
    def __init__(self):
        self.queue = queue.Queue(maxsize=EVENT_QUEUE_LIMIT)
        self.failure = None
        self.reader = None

    def put(self, item):
        try:
            self.queue.put_nowait(item)
        except queue.Full:
            self.mark_failure(FloodError("engine event queue exceeded its bounded limit"))
            return False
        return True

    def mark_failure(self, failure):
        if self.failure is None:
            self.failure = failure


class EngineStdoutReader(threading.Thread):
    """Bounded JSONL reader: line-too-long and flood become failures."""

    def __init__(self, stream, queue_bound, filter_event=None):
        super().__init__(daemon=True)
        self.stream = stream
        self.queue = queue_bound
        self.total = 0
        self.filter_event = filter_event

    def run(self):
        buffer = bytearray()
        try:
            while True:
                try:
                    chunk = self.stream.read1(CHUNK)
                except OSError:
                    break
                if not chunk:
                    break
                self.total += len(chunk)
                if self.total > ENGINE_OUTPUT_LIMIT:
                    self.queue.mark_failure(FloodError(
                        f"engine stdout exceeded its bounded capture limit "
                        f"({ENGINE_OUTPUT_LIMIT} bytes)"))
                    return
                buffer.extend(chunk)
                if b"\n" not in buffer and len(buffer) > ENGINE_LINE_LIMIT:
                    self.queue.mark_failure(FloodError(
                        f"engine line exceeds the {ENGINE_LINE_LIMIT} byte bound "
                        "before any newline"))
                    return
                while b"\n" in buffer:
                    line, _, rest = bytes(buffer).partition(b"\n")
                    buffer = bytearray(rest)
                    if len(line) > ENGINE_LINE_LIMIT:
                        self.queue.mark_failure(FloodError(
                            f"engine line longer than the {ENGINE_LINE_LIMIT} byte bound"))
                        return
                    if not self._offer(line):
                        return
            if buffer:
                self._offer(bytes(buffer))
            self.queue.put(("eof", None))
        except Exception as error:  # defensive: the reader must not exit silently
            self.queue.mark_failure(ControlledError(f"engine reader failed: {error}"))
            self.queue.put(("eof", None))

    def _offer(self, line):
        try:
            document = json.loads(line.decode("utf-8"))
        except ValueError as error:
            self.queue.mark_failure(
                ControlledError(f"engine emitted invalid JSON: {error}"))
            self.queue.put(("eof", None))
            return False
        if not isinstance(document, dict):
            self.queue.mark_failure(
                ControlledError("engine emitted a non-object JSON document"))
            self.queue.put(("eof", None))
            return False
        if self.filter_event is not None and self.filter_event(document):
            return True
        return self.queue.put(("event", document))



def _wait_bounded(thread):
    if thread is not None and thread.is_alive():
        try:
            thread.join(timeout=5.0)
        except RuntimeError:
            pass
