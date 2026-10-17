# SPDX-License-Identifier: GPL-3.0-or-later
"""WSL engine adapter proxy for the compare_game controller."""
import base64
import json
import math
import os
from pathlib import Path
import queue
import subprocess
import threading
import time
import uuid
from compare_common import ControlledError
from compare_process import OwnedProcess


class WslEngine:
    """Owned Windows launcher plus an independently bounded Linux supervisor."""

    def __init__(self, native, config, duration):
        self.native = native
        self.process = native.process
        self.pipe = self.process.stdin
        self.nonce = uuid.uuid4().hex
        self.frames = queue.Queue(maxsize=64)
        self.failure = None
        self.clock = None
        self.received = None
        self.last_renew = 0.0
        self.closed = threading.Event()
        self.clean = False
        self.stopping = False
        self.writer = threading.Thread(target=self._write_frames, daemon=True)
        self._enqueue({"op": "start", "nonce": self.nonce, "command": config["argv"],
                       "cwd": config["cwd"], "timeout": duration})
        self.writer.start()
        # Swap last: if anything above raised, native.kill_tree() still closes the
        # real stdin pipe instead of this proxy's no-op close().
        self.process.stdin = self

    @classmethod
    def launch(cls, config, duration):
        source = Path(__file__).with_name("wsl_engine.py").read_text(encoding="utf-8")
        # Pass source directly to Python; no shell expansion, path guessing,
        # helper deployment, service installation or network listener is needed.
        argv = ["wsl.exe", "--distribution", config["distribution"], "--exec",
                "/usr/bin/python3", "-u", "-c", source]
        if len(subprocess.list2cmdline(argv)) > 30000:  # CreateProcess limit is 32767
            raise ControlledError("the WSL adapter source no longer fits on a command line")
        native = OwnedProcess._launch_windows(argv, Path(__file__).resolve().parent, allow_wsl=True)
        try:
            return cls(native, config, duration)
        except BaseException:
            native.kill_tree()
            raise

    def _enqueue(self, frame):
        data = (json.dumps(frame, allow_nan=False) + "\n").encode()
        if len(data) > 128 * 1024:
            raise ControlledError("WSL adapter input exceeded its bounded frame size")
        try:
            self.frames.put_nowait(data)
        except queue.Full as error:
            raise ControlledError("WSL adapter input queue exceeded its bound") from error

    def _write_frames(self):
        while True:
            data = self.frames.get()
            if data is None:
                return
            try:
                remaining = memoryview(data)
                while remaining:
                    count = os.write(self.pipe.fileno(), remaining)
                    remaining = remaining[count:]
            except (OSError, ValueError) as error:
                self.failure = ControlledError(f"WSL adapter input failed: {error}")
                return

    def write(self, data):
        self._enqueue({"op": "input", "data": base64.b64encode(data).decode("ascii")})
        return len(data)

    def flush(self):
        if self.failure is not None:
            raise self.failure

    def close(self):
        # OwnedProcess closes this proxy only after killing the Windows job.
        pass

    def heartbeat(self):
        if self.failure is not None:
            raise self.failure
        now = time.monotonic()
        if self.clock is not None and not self.closed.is_set() and now - self.last_renew >= 0.5:
            # An absolute Linux-clock expiry prevents buffered, stale heartbeats
            # from extending a lease after the Windows controller has stalled.
            until = self.clock + (now - self.received) + 5.0
            self._enqueue({"op": "renew", "until": until})
            self.last_renew = now

    def filter_event(self, document):
        if document.get("type") != "_goldbox_wsl":
            return False
        if document.get("nonce") != self.nonce:
            raise ControlledError("WSL adapter session identity mismatch")
        phase = document.get("phase")
        if phase == "ready" and self.clock is None:
            clock = document.get("clock")
            if isinstance(clock, bool) or not isinstance(clock, (int, float)) or not math.isfinite(clock):
                raise ControlledError("WSL adapter supplied an invalid clock")
            self.received = time.monotonic()
            self.clock = clock  # Publish only after the reader's clock anchor is complete.
        elif phase == "closed" and self.clock is not None and not self.closed.is_set():
            self.clean = document.get("clean") is True
            self.closed.set()
            if not self.clean:
                raise ControlledError("WSL adapter could not verify Linux cleanup")
            engine_exit = document.get("engine_exit")
            if not self.stopping and (document.get("reason") != "engine exited"
                    or isinstance(engine_exit, bool) or not isinstance(engine_exit, int) or engine_exit != 0):
                raise ControlledError(f"WSL engine stopped: {document.get('reason')}; exit {engine_exit}")
        else:
            raise ControlledError("WSL adapter control event is out of order")
        return True

    def kill_tree(self):
        self.stopping = True
        errors = []
        try:
            if self.process.poll() is None:
                try:
                    self._enqueue({"op": "stop"})
                except ControlledError:
                    pass  # The independent Linux lease is still enforced.
                try:
                    self.process.wait(timeout=8)
                except subprocess.TimeoutExpired:
                    errors.append("WSL adapter did not exit within its cleanup budget")
            if not self.closed.wait(timeout=0.5) or not self.clean:
                errors.append("Linux cleanup acknowledgment was not received")
        finally:
            try:
                self.native.kill_tree()
            finally:
                # The writer owns the fd: stop and join it before closing, so it
                # can never write to a reused descriptor number.
                try:
                    self.frames.put_nowait(None)
                except queue.Full:
                    pass
                self.writer.join(timeout=1)
                if not self.writer.is_alive():
                    self.pipe.close()
        if errors:
            raise ControlledError("; ".join(errors))
