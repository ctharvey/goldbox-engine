# SPDX-License-Identifier: GPL-3.0-or-later
"""Bounded synthetic Linux supervisor tests; also exercises WSL from Windows."""
import base64
import json
import os
from pathlib import Path
import queue
import subprocess
import sys
import tempfile
import threading
import time
import uuid

ROOT = Path(__file__).resolve().parent.parent
SOURCE = (ROOT / "scripts/wsl_engine.py").read_text(encoding="utf-8")
CHILD = r'''
import json, subprocess, sys, time
mode = sys.argv[1]
if mode in ('tree', 'leader'):
    grand = subprocess.Popen([sys.executable, '-c', 'import time; time.sleep(10)'])
    print(json.dumps({'type':'fixture', 'engine':__import__('os').getpid(), 'grandchild':grand.pid}), flush=True)
    if mode == 'leader':
        sys.exit(0)
if mode == 'echo':
    for line in sys.stdin:
        request = json.loads(line)
        print(json.dumps({'type':'echo', 'request':request}), flush=True)
        if request.get('action') == 'quit':
            sys.exit(0)
elif mode == 'flood':
    for _ in range(1024):
        sys.stdout.write('x' * 65536)
        sys.stdout.flush()
else:
    time.sleep(10)
'''


def linux_path(path):
    path = str(Path(path).resolve())
    return "/mnt/" + path[0].lower() + "/" + path[3:].replace("\\", "/") if os.name == "nt" else path


class Probe:
    active = set()

    def __init__(self, root, mode="echo", duration=8, command=None, cwd=None):
        self.nonce = uuid.uuid4().hex
        argv = (["wsl.exe", "-d", "Ubuntu", "--exec", "/usr/bin/python3", "-u", "-c", SOURCE]
                if os.name == "nt" else [sys.executable, "-u", "-c", SOURCE])
        self.process = subprocess.Popen(argv, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                        stderr=subprocess.PIPE)
        self.active.add(self)
        self.events = queue.Queue()
        self.stderr = bytearray()
        self.reader = threading.Thread(target=self.read, daemon=True)
        self.error_reader = threading.Thread(target=self.drain_error, daemon=True)
        self.reader.start()
        self.error_reader.start()
        self.send({"op": "start", "nonce": self.nonce, "cwd": cwd or linux_path(root),
                   "command": command or ["/usr/bin/python3", "-u", "-c", CHILD, mode],
                   "timeout": duration})

    def read(self):
        for line in self.process.stdout:
            if len(line) > 1024 * 1024:
                self.events.put({"error": "unbounded fixture line"})
                break
            self.events.put(json.loads(line))
        self.events.put(None)

    def drain_error(self):
        while True:
            chunk = self.process.stderr.read1(65536)
            if not chunk:
                break
            if len(self.stderr) < 65536:
                self.stderr.extend(chunk)

    def send(self, frame):
        self.process.stdin.write((json.dumps(frame) + "\n").encode())
        self.process.stdin.flush()

    def event(self, timeout=7):
        event = self.events.get(timeout=timeout)
        assert event is not None, (self.process.poll(), self.stderr.decode(errors="replace"))
        return event

    def ready(self):
        event = self.event()
        assert event["phase"] == "ready" and event["nonce"] == self.nonce, event
        return event["clock"]

    def closed(self, reason):
        event = self.event()
        assert event["phase"] == "closed" and event["clean"] is True, event
        assert event["reason"] == reason, event
        assert self.process.wait(timeout=4) == 0
        return event

    def close(self):
        if self.process.poll() is None:
            self.process.stdin.close()
            try:
                self.process.wait(timeout=8)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait(timeout=3)
        self.reader.join(timeout=1)
        self.error_reader.join(timeout=1)
        for stream in (self.process.stdin, self.process.stdout, self.process.stderr):
            stream.close()
        self.active.discard(self)


def running(pids):
    # Use a short standalone command so the verifier is outside the owned group.
    code = '''from pathlib import Path
import json,sys
r={}
for pid in json.loads(sys.argv[1]):
    p=Path('/proc/'+str(pid)+'/stat')
    try:
        r[pid]=p.read_text().rsplit(')',1)[1].split()[0]!='Z'
    except FileNotFoundError:
        r[pid]=False
print(json.dumps(r))
'''
    argv = (["wsl.exe", "-d", "Ubuntu", "--exec", "timeout", "3", "/usr/bin/python3"]
            if os.name == "nt" else [sys.executable])
    result = subprocess.run(argv + ["-c", code, json.dumps(pids)], capture_output=True,
                            text=True, timeout=5)
    assert result.returncode == 0, result.stderr
    return json.loads(result.stdout)


def test_flow(root):
    probe = Probe(root)
    probe.ready()
    for action in ("search", "quit"):
        request = {"id": 1, "action": action, "text": "spaces & dollar $ quotes ' Unicode \u00e9"}
        data = (json.dumps(request) + "\n").encode()
        probe.send({"op": "input", "data": base64.b64encode(data).decode()})
        assert probe.event() == {"type": "echo", "request": request}
    assert probe.closed("engine exited")["engine_exit"] == 0
    probe.close()


def test_cleanup(root):
    for mode, operation, reason in (("tree", "stop", "controller stopped"),
                                    ("tree", "eof", "controller input closed"),
                                    ("tree", "deadline", "session deadline"),
                                    ("leader", "leader", "engine exited")):
        probe = Probe(root, mode, duration=2 if operation == "deadline" else 8)
        probe.ready()
        pids = probe.event()
        if operation == "stop":
            probe.send({"op": "stop"})
        elif operation == "eof":
            probe.process.stdin.close()
        probe.closed(reason)
        assert not any(running([pids["engine"], pids["grandchild"]]).values())
        probe.close()


def test_lease(root):
    probe = Probe(root, "tree", duration=9)
    clock = probe.ready()
    pids = probe.event()
    # Stale queued renewals cannot keep the helper alive beyond the initial lease.
    probe.send({"op": "renew", "until": clock + 1})
    probe.closed("controller lease expired")
    assert not any(running([pids["engine"], pids["grandchild"]]).values())
    probe.close()


def test_lease_renewal(root):
    probe = Probe(root, "tree", duration=9)
    clock = probe.ready()
    received = time.monotonic()
    probe.event()
    end = received + 6
    while time.monotonic() < end:
        probe.send({"op": "renew", "until": clock + time.monotonic() - received + 5})
        time.sleep(0.5)
        assert probe.process.poll() is None, probe.stderr.decode(errors="replace")
    probe.send({"op": "stop"})
    probe.closed("controller stopped")
    probe.close()


def test_lease_renewal_under_clock_drift(root):
    # N8: a slow Linux clock makes the controller's absolute expiry run ahead of
    # the adapter's receipt time. Renewals must keep working, and a far-future
    # claim made right after ready must still be rejected (test_input_bounds).
    probe = Probe(root, "tree", duration=9)
    clock = probe.ready()
    received = time.monotonic()
    probe.event()
    end = received + 7
    while time.monotonic() < end:
        elapsed = time.monotonic() - received
        probe.send({"op": "renew", "until": clock + elapsed * 1.08 + 5})
        time.sleep(0.5)
        assert probe.process.poll() is None, probe.stderr.decode(errors="replace")
    probe.send({"op": "stop"})
    probe.closed("controller stopped")
    probe.close()


def test_supervisor_loss(root):
    probe = Probe(root, "tree")
    probe.ready()
    pids = probe.event()
    probe.process.kill()  # Linux supervisor death, or Windows launcher death.
    probe.process.wait(timeout=3)
    limit = time.monotonic() + 7
    while any(running([pids["engine"], pids["grandchild"]]).values()):
        assert time.monotonic() < limit, "children survived supervisor/launcher loss"
        time.sleep(0.1)
    probe.close()


def test_invalid_start(root):
    for command, cwd in ((["/no/goldbox/fixture"], None), (None, "/no/goldbox/directory")):
        probe = Probe(root, command=command, cwd=cwd)
        assert probe.process.wait(timeout=7) in (0, 1)
        if cwd is None:
            probe.ready()
            probe.closed("cannot start or supervise engine")
        else:
            probe.error_reader.join(timeout=1)
            assert b"Linux cwd" in probe.stderr
        probe.close()


def test_input_bounds(root):
    for operation in ("future-lease", "blocked-input"):
        probe = Probe(root, "tree")
        clock = probe.ready()
        pids = probe.event()
        if operation == "future-lease":
            probe.send({"op": "renew", "until": clock + 100})
            diagnostics = (b"invalid lease deadline",)
        else:
            data = base64.b64encode(b"x" * 65536).decode()
            for _ in range(64):
                try:
                    probe.send({"op": "input", "data": data})
                except BrokenPipeError:
                    break
            diagnostics = (b"engine input buffer exceeded bound", b"adapter control input exceeded bound")
        code = probe.process.wait(timeout=6)
        assert code == 1, (operation, code, probe.stderr)
        probe.error_reader.join(timeout=1)
        assert any(diagnostic in probe.stderr for diagnostic in diagnostics), probe.stderr
        assert not any(running([pids["engine"], pids["grandchild"]]).values())
        probe.close()


def test_hostile_frame_and_signal_policy(root):
    # F24: a hostile control frame ends the adapter with a diagnostic, not a
    # traceback, and cleans up. F23: the engine must not inherit SIGHUP=SIG_IGN.
    probe = Probe(root, "tree")
    probe.ready()
    pids = probe.event()
    probe.process.stdin.write(b"[" * 5000 + b"]" * 5000 + b"\n")
    probe.process.stdin.flush()
    assert probe.process.wait(timeout=8) == 1, probe.stderr
    probe.error_reader.join(timeout=1)
    assert b"Traceback" not in probe.stderr and b"WSL adapter:" in probe.stderr, probe.stderr
    assert not any(running([pids["engine"], pids["grandchild"]]).values())
    probe.close()

    report = ("import signal, json, time\n"
              "print(json.dumps({'ignored': signal.getsignal(signal.SIGHUP) == signal.SIG_IGN}),"
              " flush=True)\ntime.sleep(5)\n")
    probe = Probe(root, command=["/usr/bin/python3", "-u", "-c", report])
    probe.ready()
    reported = probe.event()
    assert reported == {"ignored": False}, reported
    probe.send({"op": "stop"})
    probe.closed("controller stopped")
    probe.close()


def test_blocked_output(root):
    # Do not drain stdout: a full Windows/Linux pipe must not defeat the deadline.
    argv = (["wsl.exe", "-d", "Ubuntu", "--exec", "/usr/bin/python3", "-u", "-c", SOURCE]
            if os.name == "nt" else [sys.executable, "-u", "-c", SOURCE])
    process = subprocess.Popen(argv, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE)
    try:
        pid_file = root / "blocked-pids.json"
        prefix = ("import os,json; from pathlib import Path; "
                  "p=os.getppid(); "
                  "supervisor=int(Path('/proc/'+str(p)+'/stat').read_text().rsplit(')',1)[1].split()[1]); "
                  f"Path({linux_path(pid_file)!r}).write_text(json.dumps([os.getpid(),p,supervisor]));\n")
        config = {"op": "start", "nonce": uuid.uuid4().hex, "cwd": linux_path(root),
                  "command": ["/usr/bin/python3", "-c", prefix + CHILD, "flood"], "timeout": 1}
        process.stdin.write((json.dumps(config) + "\n").encode())
        process.stdin.flush()
        limit = time.monotonic() + 5
        while not pid_file.exists():
            assert time.monotonic() < limit, "blocked-output engine did not start"
            time.sleep(0.05)
        pids = json.loads(pid_file.read_text())
        limit = time.monotonic() + 5
        while any(running(pids).values()):
            assert time.monotonic() < limit, "blocked output defeated Linux cleanup deadline"
            time.sleep(0.1)
        if os.name != "nt":
            assert process.wait(timeout=2) == 1
    finally:
        if process.poll() is None:
            process.kill()
        process.wait(timeout=3)
        for stream in (process.stdin, process.stdout, process.stderr):
            stream.close()


def test_windows_controller(root):
    from compare_game_test import Fixture, ENGINE_SOURCE

    fixture = Fixture(root)
    controller = str(ROOT / "scripts/compare_game.py")

    def config(mode="rich", duration=15, command=None):
        return fixture.config(mode, timeout=duration, checkpointSeconds=5, engine={
            "backend": "wsl", "distribution": "Ubuntu", "cwd": linux_path(fixture.engine_dir),
            "command": command or ["/usr/bin/python3", linux_path(fixture.engine), mode]})

    path = config()
    run = subprocess.run([sys.executable, controller, str(path)],
        input="search\nnorth\neast\nsearch\n1\nquit\n", text=True, capture_output=True, timeout=20)
    assert run.returncode == 0, (run.stdout, run.stderr)
    assert "chosen menu entry 1" in run.stdout and "no comparison result" in run.stdout
    assert "_goldbox_wsl" not in run.stdout

    # Waiting for operator input beyond the initial lease must renew the lease.
    process = subprocess.Popen([sys.executable, "-u", controller, str(path)], stdin=subprocess.PIPE,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    output = []
    ready = threading.Event()

    def drain():
        for line in process.stdout:
            output.append(line)
            if line.startswith("[engine ready]"):
                ready.set()

    reader = threading.Thread(target=drain, daemon=True)
    reader.start()
    try:
        assert ready.wait(timeout=7), "WSL controller did not become ready"
        time.sleep(6)
        process.stdin.write("quit\n")
        process.stdin.flush()
        process.wait(timeout=10)
        reader.join(timeout=2)
        assert process.returncode == 0, ("".join(output), process.stderr.read(65536))
    finally:
        if process.poll() is None:
            process.kill()
            process.wait(timeout=3)
        reader.join(timeout=1)
        for stream in (process.stdin, process.stdout, process.stderr):
            stream.close()

    failed = subprocess.run([sys.executable, controller,
        str(config(command=["/no/goldbox/engine"]))], input="quit\n", text=True,
        capture_output=True, timeout=15)
    assert failed.returncode != 0 and "cannot start or supervise engine" in failed.stderr, failed.stderr

    # Kill the controller while the engine and a grandchild are alive. An
    # independent Linux session stays alive throughout this session's cleanup.
    sentinel = Probe(root, "tree", duration=9)
    sentinel.ready()
    sentinel_pids = sentinel.event()
    fixture.engine.write_text(ENGINE_SOURCE.replace("time.sleep(0.3)", "time.sleep(10)"), encoding="utf-8")
    crash_config = config("spawn-then-die", command=["/usr/bin/python3", linux_path(fixture.engine),
                           "spawn-then-die", linux_path(fixture.pid_file)])
    crashed = subprocess.Popen([sys.executable, controller, str(crash_config)], stdin=subprocess.PIPE,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    try:
        limit = time.monotonic() + 7
        while not fixture.pid_file.exists():
            assert crashed.poll() is None and time.monotonic() < limit, "controller failed to start"
            time.sleep(0.05)
        child_pid = int(fixture.pid_file.read_text())
        crashed.kill()
        crashed.wait(timeout=3)
        limit = time.monotonic() + 7
        while any(running([child_pid]).values()):
            assert time.monotonic() < limit, "controller crash leaked a Linux child"
            time.sleep(0.1)
        assert all(running([sentinel_pids["engine"], sentinel_pids["grandchild"]]).values())
    finally:
        if crashed.poll() is None:
            crashed.kill()
            crashed.wait(timeout=3)
        for stream in (crashed.stdin, crashed.stdout, crashed.stderr):
            stream.close()
        sentinel.close()


def test_windows_compiled_engine(root, executable, profile):
    from compare_game_test import Fixture
    from harness_test import dax

    fixture = Fixture(root)
    geo = bytearray(1026)
    geo[515] = 0x81
    geo_path, ecl_path = fixture.private / "GEO.DAX", fixture.private / "ECL.DAX"
    geo_path.write_bytes(dax(bytes(geo)))
    ecl_path.write_bytes(dax(bytes(
        [1, 1, 20, 0x99] * 5 + [9, 1, 0x4f, 0xc0, 1, 0, 0x4a, 0x11, 0x80, 1, 4, 0])))
    save = fixture.private / "state.json"
    path = fixture.config("rich", timeout=15, checkpointSeconds=5, engine={
        "backend": "wsl", "distribution": "Ubuntu", "cwd": linux_path(fixture.private),
        "command": [executable, "--harness", profile, linux_path(geo_path), "7", linux_path(ecl_path),
                    "7", linux_path(save)]})
    result = subprocess.run([sys.executable, str(ROOT / "scripts/compare_game.py"), str(path)],
        input="east\nnorth\nsave\nquit\n", text=True, capture_output=True, timeout=20)
    assert result.returncode == 0, (result.stdout, result.stderr)
    assert "status=blocked" in result.stdout and "\nA\n" in result.stdout
    assert json.loads(save.read_text())["memory"][0x4a00] == 0x81


def main():
    try:
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            for test in (test_flow, test_cleanup, test_lease, test_lease_renewal, test_lease_renewal_under_clock_drift,
                         test_supervisor_loss,
                         test_invalid_start, test_input_bounds, test_hostile_frame_and_signal_policy,
                         test_blocked_output):
                test(root)
                print(test.__name__ + ": PASS", flush=True)
            if os.name == "nt":
                controller_root = root / "controller"
                controller_root.mkdir()
                test_windows_controller(controller_root)
                print("test_windows_controller: PASS", flush=True)
                if len(sys.argv) == 3:
                    compiled_root = root / "compiled"
                    compiled_root.mkdir()
                    test_windows_compiled_engine(compiled_root, sys.argv[1], sys.argv[2])
                    print("test_windows_compiled_engine: PASS", flush=True)
    finally:
        for probe in list(Probe.active):
            probe.close()


if __name__ == "__main__":
    main()
