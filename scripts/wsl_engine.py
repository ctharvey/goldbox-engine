#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Linux side of the comparison adapter; private bounded pipe protocol.

Independently authored from Python subprocess/selectors and Linux process-group
documentation. No third-party implementation is copied. The anchor remains an
unreaped child until cleanup, so its process-group identifier cannot be reused.
"""
import base64
import json
import math
import os
from pathlib import Path
import selectors
import signal
import subprocess
import sys
import time

LEASE_SECONDS = 5.0
CLEANUP_SECONDS = 3.0  # Must stay below the controller's 8 s wait for this process.
REAP_SECONDS = 2.0
INPUT_LIMIT = 128 * 1024
BUFFER_LIMIT = 1024 * 1024
OUTPUT_LIMIT = 64 * 1024 * 1024
CHUNK = 65536


def encode(value):
    return (json.dumps(value, allow_nan=False) + "\n").encode()


def group_running(group):
    for entry in Path("/proc").iterdir():
        if not entry.name.isdecimal():
            continue
        try:
            fields = (entry / "stat").read_text().rsplit(")", 1)[1].split()
            if int(fields[2]) == group and fields[0] != "Z":
                return True
        except (FileNotFoundError, ProcessLookupError):
            continue
    return False


def anchor(command, cwd, pipes, parent, deadline):
    """Own a session, retain its ID, and also stop it if the supervisor dies."""
    os.setsid()
    stdin_read, stdin_write, out_read, out_write, err_read, err_write, status_read, status_write = pipes
    for descriptor in (0, 1, 2, stdin_write, out_read, err_read, status_read):
        os.close(descriptor)

    def stop(*_):
        os.killpg(os.getpid(), signal.SIGKILL)

    signal.signal(signal.SIGTERM, stop)
    signal.signal(signal.SIGHUP, signal.SIG_IGN)
    signal.signal(signal.SIGALRM, stop)
    signal.setitimer(signal.ITIMER_REAL, max(0.1, deadline - time.monotonic()))
    try:
        # SIG_IGN survives exec; the engine must not inherit this process's SIGHUP policy.
        child = subprocess.Popen(command, cwd=cwd, stdin=stdin_read, stdout=out_write,
                                 stderr=err_write, close_fds=True,
                                 preexec_fn=lambda: signal.signal(signal.SIGHUP, signal.SIG_DFL))
        for descriptor in (stdin_read, out_write, err_write):
            os.close(descriptor)
        reported = False
        while True:
            if os.getppid() != parent:
                stop()
            code = child.poll()
            if code is not None and not reported:
                os.write(status_write, encode({"exit": code}))
                reported = True
            time.sleep(0.05)
    except BaseException:
        try:
            os.write(status_write, encode({"error": "cannot start or supervise engine"}))
        finally:
            stop()


def read_start():
    """Read one bounded initial frame without permitting an idle orphan."""
    os.set_blocking(0, False)
    selector = selectors.DefaultSelector()
    selector.register(0, selectors.EVENT_READ)
    buffer = bytearray()
    deadline = time.monotonic() + LEASE_SECONDS
    try:
        while time.monotonic() < deadline:
            if not selector.select(0.1):
                continue
            chunk = os.read(0, CHUNK)
            if not chunk:
                raise ValueError("input closed before adapter start")
            buffer.extend(chunk)
            if len(buffer) > INPUT_LIMIT:
                raise ValueError("adapter start exceeds input bound")
            if b"\n" in buffer:
                line, _, rest = buffer.partition(b"\n")
                config = json.loads(line)
                if not isinstance(config, dict) or config.get("op") != "start":
                    raise ValueError("expected adapter start")
                command = config.get("command")
                if not isinstance(command, list) or not command or any(
                        not isinstance(arg, str) or not arg or "\0" in arg for arg in command):
                    raise ValueError("invalid Linux command")
                cwd = config.get("cwd")
                if not isinstance(cwd, str) or not cwd.startswith("/") or not Path(cwd).is_dir():
                    raise ValueError("Linux cwd must be an existing absolute directory")
                duration = config.get("timeout")
                if isinstance(duration, bool) or not isinstance(duration, (int, float)) \
                        or not math.isfinite(duration) or not 1 <= duration <= 600:
                    raise ValueError("invalid adapter deadline")
                nonce = config.get("nonce")
                if not isinstance(nonce, str) or len(nonce) != 32 \
                        or any(char not in "0123456789abcdef" for char in nonce):
                    raise ValueError("invalid adapter session identity")
                return config, bytearray(rest)
        raise ValueError("adapter start deadline expired")
    finally:
        selector.close()


def supervise(config, incoming):
    signal.signal(signal.SIGHUP, signal.SIG_IGN)
    now = time.monotonic()
    deadline, lease = now + config["timeout"], now + LEASE_SECONDS
    pending_out = bytearray(encode({"type": "_goldbox_wsl", "nonce": config["nonce"],
                                   "phase": "ready", "clock": now}))
    pending_err, pending_in, status = bytearray(), bytearray(), bytearray()
    pipes = (*os.pipe(), *os.pipe(), *os.pipe(), *os.pipe())
    stdin_read, stdin_write, out_read, out_write, err_read, err_write, status_read, status_write = pipes
    parent = os.getpid()
    pid = os.fork()
    if pid == 0:
        anchor(config["command"], config["cwd"], pipes, parent, deadline + 2)
        os._exit(1)
    for descriptor in (stdin_read, out_write, err_write, status_write):
        os.close(descriptor)
    for descriptor in (0, 1, 2, stdin_write, out_read, err_read, status_read):
        os.set_blocking(descriptor, False)
    selector = selectors.DefaultSelector()
    engine_exit = None
    reason = None
    cleaned = False
    closed_sent = False
    out_open = err_open = status_open = True
    total_out = total_err = 0
    drain_deadline = None
    last_stdout_byte = b"\n"

    def cleanup():
        nonlocal cleaned, drain_deadline
        # The child may not have called setsid yet. Signal the reserved child
        # PID as well; no wait/reap occurs until all group verification is done.
        try:
            os.kill(pid, signal.SIGTERM)
        except ProcessLookupError:
            pass
        limit = time.monotonic() + CLEANUP_SECONDS
        while time.monotonic() < limit:
            try:
                os.killpg(pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            try:
                os.kill(pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            if not group_running(pid):
                cleaned = True
                break
            time.sleep(0.01)
        drain_deadline = time.monotonic() + 2

    try:
        while True:
            now = time.monotonic()
            if reason is None:
                if now >= deadline:
                    reason = "session deadline"
                elif now >= lease:
                    reason = "controller lease expired"
                elif engine_exit is not None:
                    reason = "engine exited"
            if reason is not None and drain_deadline is None:
                cleanup()
            if reason is not None and not out_open and not err_open and not closed_sent:
                if last_stdout_byte != b"\n":
                    pending_out.extend(b"\n")
                pending_out.extend(encode({"type": "_goldbox_wsl", "nonce": config["nonce"],
                    "phase": "closed", "clean": cleaned, "reason": reason,
                    "engine_exit": engine_exit}))
                closed_sent = True
            if closed_sent and not pending_out and not pending_err:
                return 0 if cleaned else 1
            if drain_deadline is not None and now >= drain_deadline:
                return 1

            # Process complete control frames before waiting for more bytes.
            while b"\n" in incoming and reason is None:
                line, _, rest = incoming.partition(b"\n")
                incoming = bytearray(rest)
                frame = json.loads(line)
                if not isinstance(frame, dict):
                    raise ValueError("invalid adapter control frame")
                op = frame.get("op")
                if op == "renew":
                    until = frame.get("until")
                    received = time.monotonic()
                    if isinstance(until, bool) or not isinstance(until, (int, float)) \
                            or not math.isfinite(until) or until > received + LEASE_SECONDS + 0.25:
                        raise ValueError(f"invalid lease deadline: {until!r}; Linux clock {time.monotonic()!r}")
                    # Windows QPC and Linux CLOCK_MONOTONIC can drift by a few
                    # milliseconds. Tolerate that, but never grant >5s from receipt.
                    lease = max(lease, min(until, received + LEASE_SECONDS))
                elif op == "input":
                    pending_in.extend(base64.b64decode(frame["data"], validate=True))
                    if len(pending_in) > INPUT_LIMIT:
                        raise ValueError("engine input buffer exceeded bound")
                elif op == "stop":
                    reason = "controller stopped"
                else:
                    raise ValueError("unknown adapter control operation")
            if reason is not None and drain_deadline is None:
                continue

            registrations = [(0, selectors.EVENT_READ, "control")] if reason is None else []
            if out_open and len(pending_out) < BUFFER_LIMIT:
                registrations.append((out_read, selectors.EVENT_READ, "out"))
            if err_open and len(pending_err) < BUFFER_LIMIT:
                registrations.append((err_read, selectors.EVENT_READ, "err"))
            if status_open:
                registrations.append((status_read, selectors.EVENT_READ, "status"))
            if pending_in and reason is None:
                registrations.append((stdin_write, selectors.EVENT_WRITE, "input"))
            if pending_out:
                registrations.append((1, selectors.EVENT_WRITE, "stdout"))
            if pending_err:
                registrations.append((2, selectors.EVENT_WRITE, "stderr"))
            for key in list(selector.get_map().values()):
                selector.unregister(key.fd)
            for descriptor, events, tag in registrations:
                selector.register(descriptor, events, tag)
            for key, _ in selector.select(0.05):
                tag, descriptor = key.data, key.fd
                if tag in ("stdout", "stderr", "input"):
                    buffer = {"stdout": pending_out, "stderr": pending_err,
                              "input": pending_in}[tag]
                    try:
                        count = os.write(descriptor, buffer[:CHUNK])
                        del buffer[:count]
                    except BlockingIOError:
                        pass
                    except BrokenPipeError:
                        if tag != "input":
                            return 1
                        reason = "engine input closed"
                    continue
                chunk = os.read(descriptor, CHUNK)
                if tag == "control":
                    if not chunk:
                        reason = "controller input closed"
                    incoming.extend(chunk)
                    if len(incoming) > INPUT_LIMIT:
                        raise ValueError("adapter control input exceeded bound")
                elif tag == "status":
                    if not chunk:
                        status_open = False
                        if engine_exit is None and reason is None:
                            reason = "engine anchor failed"
                    status.extend(chunk)
                    if len(status) > INPUT_LIMIT:
                        raise ValueError("anchor status exceeded bound")
                    if b"\n" in status:
                        message = json.loads(status.partition(b"\n")[0])
                        if not isinstance(message, dict):
                            raise ValueError("invalid anchor status")
                        engine_exit = message.get("exit")
                        if "error" in message:
                            reason = message["error"]
                elif tag == "out":
                    if not chunk:
                        out_open = False
                    else:
                        last_stdout_byte = chunk[-1:]
                    total_out += len(chunk)
                    pending_out.extend(chunk)
                else:
                    if not chunk:
                        err_open = False
                    total_err += len(chunk)
                    pending_err.extend(chunk)
                if total_out > OUTPUT_LIMIT or total_err > OUTPUT_LIMIT:
                    raise ValueError("engine output exceeded adapter bound")
    finally:
        if drain_deadline is None:
            cleanup()
        reap_limit = time.monotonic() + REAP_SECONDS
        while time.monotonic() < reap_limit:  # A D-state anchor must not hang us; init reaps it.
            try:
                if os.waitpid(pid, os.WNOHANG)[0] == pid:
                    break
            except ChildProcessError:
                break
            time.sleep(0.01)
        selector.close()
        for descriptor in (stdin_write, out_read, err_read, status_read):
            os.close(descriptor)


def main():
    if not sys.platform.startswith("linux"):
        return 1
    try:
        config, incoming = read_start()
        return supervise(config, incoming)
    except Exception as error:  # incl. RecursionError/AttributeError from hostile frames
        try:
            os.write(2, ("WSL adapter: " + str(error) + "\n").encode())
        except OSError:
            pass
        return 1


if __name__ == "__main__":
    sys.exit(main())
