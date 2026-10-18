# SPDX-License-Identifier: GPL-3.0-or-later
"""Synthetic tests for the bounded compare_game controller via the CLI entry point.

No original games, assets or proprietary text are used; every child is a
synthetic Python fixture. Orchestration follows harness_test.py style:
assert-based functions invoked from main().
"""
import ctypes
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import time

CONTROLLER = Path(__file__).resolve().parent.parent / "scripts" / "compare_game.py"
WALL_LIMIT = 10.0

ENGINE_SOURCE = r'''
import json
import os
import subprocess
import sys
import time


def emit(payload):
    sys.stdout.write(json.dumps(payload) + "\n")
    sys.stdout.flush()


def complete(identifier, status, row=0, column=0, facing=0, diagnostic=""):
    return {"type": "complete", "id": identifier, "status": status, "row": row,
            "column": column, "facing": facing, "steps": 1, "bytecode_offset": 2,
            "dispatches": [{"table_offset": 8, "selector": 1, "target_offset": 12,
                            "subroutine": False}] if status == "ok" else [],
            "diagnostic": diagnostic,
            "state": {"schemaVersion": 1, "dialect": "synthetic", "memory": [0, 1, 2]}}


READY = {"type": "ready", "protocol": 1, "source_identity": "synthetic:fake:engine",
         "row": 0, "column": 0, "facing": 0, "baseline": "unverified-empty"}
scenario = sys.argv[1]

if scenario == "invalidready":
    emit(dict(READY, protocol=2))
    time.sleep(2.0)
    sys.exit(2)

if scenario == "spawn-then-die":
    grand = subprocess.Popen([sys.executable, "-c", "import time; time.sleep(600)"])
    with open(sys.argv[2], "w", encoding="ascii") as handle:
        handle.write(str(grand.pid))
    emit(READY)
    time.sleep(0.3)
    sys.exit(0)

if scenario == "writer":
    destination = sys.argv[2]
    with open(destination, "w", encoding="utf-8") as handle:
        handle.write(os.getcwd() + "\n")
        handle.write(json.dumps(sys.argv[2:]) + "\n")
    emit(READY)
    for line in sys.stdin:
        request = json.loads(line)
        if request["action"] == "quit":
            emit(complete(request["id"], "ok"))
            sys.exit(0)
        emit(complete(request["id"], "ok"))
    sys.exit(0)

if scenario == "die-early":
    emit(READY)
    time.sleep(0.2)
    sys.exit(3)

if scenario == "error-event":
    emit(READY)
    emit({"type": "error", "message": "boom"})
    time.sleep(1.0)
    sys.exit(1)

if scenario == "wrong-id":
    emit(READY)
    line = sys.stdin.readline()
    request = json.loads(line)
    emit(dict(complete(request["id"], "ok"), id=999))
    time.sleep(2.0)
    sys.exit(1)

if scenario == "spontaneous":
    emit(READY)
    emit({"type": "text", "id": 1, "text": "early", "clear": False})
    time.sleep(2.0)
    sys.exit(1)

if scenario == "flood":
    emit(READY)
    while True:
        sys.stderr.write("flooded" * 16384)
        sys.stderr.flush()

if scenario == "overlong":
    sys.stdout.write(json.dumps({"type": "ready", "junk": "z" * (9 * 1024 * 1024)}) + "\n")
    sys.stdout.flush()
    time.sleep(5.0)
    sys.exit(0)

if scenario == "hang":
    emit(READY)
    while True:
        time.sleep(0.1)
    sys.exit(0)

if scenario.startswith("bad-"):
    emit(READY)
    request = json.loads(sys.stdin.readline())
    event = complete(request["id"], "ok")
    if scenario == "bad-position":
        event["row"] = True
    elif scenario == "bad-state":
        event["state"] = []
    elif scenario == "bad-dispatch":
        event["dispatches"][0]["target_offset"] = -1
    elif scenario == "bad-quit":
        event["status"] = "unsupported"
    elif scenario == "bad-menu":
        event = {"type": "menu", "id": request["id"], "menu_id": True,
                 "prompt": "synthetic", "choices": ["Alpha"], "vertical": True}
    emit(event)
    sys.exit(0)

if scenario == "rich":
    searches = 0
    emit(READY)
    position = {"row": 0, "column": 0, "facing": 0}
    for line in sys.stdin:
        request = json.loads(line)
        identifier = request["id"]
        action = request["action"]
        if action == "quit":
            emit(complete(identifier, "ok", **position))
            sys.exit(0)
        if action == "search":
            emit({"type": "text", "id": identifier,
                  "text": "synthetic search stub text", "clear": False})
            searches += 1
            if searches < 2:
                emit(complete(identifier, "ok", **position))
                continue
            emit({"type": "menu", "id": identifier, "menu_id": 1,
                  "prompt": "synthetic menu", "choices": ["Alpha", "Beta"],
                  "vertical": True})
            response = json.loads(sys.stdin.readline())
            emit({"type": "text", "id": identifier,
                  "text": f"chosen menu entry {response['choice']}", "clear": False})
            emit(complete(identifier, "ok", **position))
            continue
        if action == "north":
            emit(complete(identifier, "blocked", **position))
            continue
        if action == "east":
            position["column"] += 1
            position["facing"] = 1
        emit(complete(identifier, "ok", **position))
    sys.exit(0)
raise SystemExit(f"unknown scenario {scenario}")
'''

REFERENCE_SOURCE = r'''
import sys
import time
if len(sys.argv) > 1 and sys.argv[1] == "fail":
    sys.stderr.write("synthetic reference failure\n")
    time.sleep(0.2)
    sys.exit(7)
while True:
    time.sleep(0.2)
'''


class Fixture:
    """One synthetic private root with engine/reference fixtures."""

    def __init__(self, temp):
        self.root = Path(temp)
        self.private = self.root / "private outside the tree"
        self.private.mkdir()
        self.engine_dir = self.root / "engine & fixture dir"
        self.engine_dir.mkdir()
        self.engine = self.engine_dir / "fake_engine.py"
        self.engine.write_text(ENGINE_SOURCE, encoding="utf-8")
        self.reference = self.root / "fake_reference.py"
        self.reference.write_text(REFERENCE_SOURCE, encoding="utf-8")
        self.fixture_marker = self.engine_dir / "fixture-marker.txt"
        self.fixture_marker.write_bytes(b"synthetic fixture only\n")
        self.pid_file = self.root / "grandchild.pid"

    def config(self, scenario, name="config.json", timeout=4, private=None,
               engine_cwd=None, reference_cwd=None, reference_argv=None, **overrides):
        settings = {
            "schemaVersion": 1,
            "privateRoot": str(private or self.private),
            "engine": {"command": [sys.executable, str(self.engine), scenario]
                       + ([str(self.pid_file)] if scenario == "spawn-then-die" else []),
                       "cwd": str(engine_cwd or self.engine_dir)},
            "reference": {"command": reference_argv or [sys.executable, str(self.reference)],
                          "cwd": str(reference_cwd or self.private)},
            "timeoutSeconds": timeout,
            "checkpointSeconds": 2,
        }
        settings.update(overrides)
        path = self.root / name
        path.write_text(json.dumps(settings), encoding="utf-8")
        return path


def run_controller(config_path, mode, extra_input="", hold_open=False):
    """Invoke the real CLI entry point; never via mocks."""
    if hold_open:
        probe = subprocess.Popen([sys.executable, str(CONTROLLER), str(config_path)],
                                 stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                 stderr=subprocess.PIPE, text=True)
        operator = probe.stdin
        try:
            operator.write(extra_input)
            operator.flush()
            probe.stdin = None  # Keep operator input open while draining both output pipes.
            stdout, stderr = probe.communicate(timeout=WALL_LIMIT)
            return subprocess.CompletedProcess(probe.args, probe.returncode, stdout, stderr)
        finally:
            if probe.poll() is None:
                probe.kill()
                probe.wait(timeout=5)
            operator.close()
    return subprocess.run(
        [sys.executable, str(CONTROLLER), str(config_path)] if mode == "normal"
        else [sys.executable, str(CONTROLLER)],
        input=extra_input, capture_output=True, text=True, timeout=WALL_LIMIT)


def check(condition, message):

    if not condition:
        raise AssertionError(message)


def test_render_flow(fixture):
    # Acceptance 1: valid ready/text/menu/complete with exact ordering.
    config = fixture.config("rich", name="rich.json", timeout=8)
    offer = run_controller(config, "normal",
                extra_input="search\nnorth\neast\nsearch\n1\nquit\n")
    try:
        check(offer.returncode == 0, f"exit {offer.returncode}: {offer.stderr}")
        markers = ["[complete id=1] status=ok",
                   "[complete id=2] status=blocked",
                   "[complete id=3] status=ok",
                   "sent action id 4: search",
                   "sent menu response: action id 4, menu id 1, choice 1",
                   "[complete id=4] status=ok",
                   "[complete id=5] status=ok"]
        offsets = [offer.stdout.find(marker) for marker in markers]
        check(all(offset >= 0 for offset in offsets), f"missing markers: {offer.stdout}")
        check(offsets == sorted(offsets), "markers out of order")
        check("no comparison result" in offer.stdout, "inconclusive disclaimer missing")
        raws = [line for line in offer.stdout.splitlines() if line.startswith('{"')]
        check(not raws, f"raw JSON leaked to the display: {raws[:1]}")
        check(offer.stderr == "", f"stderr noise: {offer.stderr!r}")
    finally:
        pass  # controller owns and kills both fake children in its finally block


def test_malformed_configs(fixture):
    # Acceptance 2: malformed config rejection happens before spawn.
    cases = [
        ("bad argv", {"engine": {"command": 42, "cwd": str(fixture.engine_dir)}}),
        ("empty argv", {"engine": {"command": [],
                                   "cwd": str(fixture.engine_dir)}}),
        ("null command", {"engine": {"command": None, "cwd": str(fixture.engine_dir)}}),
        ("unsupported schema", {"schemaVersion": 2}),
        ("bool schema", {"schemaVersion": True}),
        ("relative root", {"privateRoot": "relative-place"}),
        ("bool timeout", {"timeoutSeconds": True}),
        ("zero timeout", {"timeoutSeconds": 0}),
        ("huge checkpoint", {"timeoutSeconds": 2, "checkpointSeconds": 3}),
    ]
    for name, override in cases:
        config = fixture.config("hang", name="bad.json", **override)
        outcome = run_controller(config, "normal")
        check(outcome.returncode == 1,
              f"{name}: expected rejection, got {outcome.returncode}: "
              f"{outcome.stderr!r}")
    nan_config = json.dumps({
        "schemaVersion": 1,
        "privateRoot": str(fixture.private.resolve()),
        "engine": {"command": [sys.executable, str(fixture.engine), "hang"],
                   "cwd": str(fixture.engine_dir)},
        "reference": {"command": [sys.executable, str(fixture.reference)],
                      "cwd": str(fixture.private)},
        "timeoutSeconds": float("nan"),
    })  # json.dumps renders the NaN literal; the controller must reject it
    nan_path = fixture.root / "nan.json"
    nan_path.write_text(nan_config, encoding="utf-8")
    outcome = run_controller(nan_path, "normal")
    check(outcome.returncode == 1 and "requires an integer" in outcome.stderr,
          f"NaN timeout rejection: {outcome.returncode} {outcome.stderr!r}")
    invalid = fixture.root / "not-json.json"
    invalid.write_text("{oops", encoding="utf-8")
    outcome = run_controller(invalid, "normal")
    check(outcome.returncode == 1 and "not valid JSON" in outcome.stderr,
          f"invalid JSON handling: {outcome.returncode} {outcome.stderr!r}")
    outcome = subprocess.run([sys.executable, str(CONTROLLER)],
                             capture_output=True, text=True, timeout=WALL_LIMIT)
    check(outcome.returncode == 2 and "usage" in outcome.stderr.lower(),
          f"usage error: {outcome.returncode} {outcome.stderr!r}")
    inside = fixture.config("hang", name="inside-repo.json",
                            private=Path(CONTROLLER).resolve().parent.parent)
    outcome = run_controller(inside, "normal")
    check("outside the repository" in outcome.stderr,
          f"repository containment missing: {outcome.stderr!r}")
    other = fixture.root / "outside-the-root"
    other.mkdir()
    stray = fixture.config("hang", name="stray-reference.json",
                           reference_cwd=other.resolve())
    outcome = run_controller(stray, "normal")
    check("reference.cwd must be inside privateRoot" in outcome.stderr,
          f"reference containment missing: {outcome.stderr!r}")
    marker = fixture.engine_dir / "fixture-marker.txt"
    check(marker.read_bytes() == b"synthetic fixture only\n",
          "fixture modified during config rejections")
    if os.name == "nt":
        # WSL launchers are rejected before any spawn: their Linux children are
        # outside every Windows Job Object this controller owns.
        for launcher in ("wsl.exe", r"C:\Windows\System32\bash.exe"):
            config = fixture.config("hang", name="wsl.json",
                                    engine={"command": [launcher, "--version"],
                                            "cwd": str(fixture.engine_dir)},
                                    reference={"command": [sys.executable,
                                                           str(fixture.reference)],
                                               "cwd": str(fixture.private)})
            outcome = run_controller(config, "normal")
            check(outcome.returncode == 1 and "unsupported backend" in outcome.stderr,
                  f"WSL launcher {launcher}: {outcome.returncode} {outcome.stderr!r}")
            check(marker.read_bytes() == b"synthetic fixture only\n",
                  f"WSL launcher {launcher}: fixture modified")


def test_protocol_failures(fixture):
    # Acceptance 3: EOF, engine and reference failures surface explicit errors.
    outcome = run_controller(fixture.config("die-early", name="die.json",
                                            timeout=6), "normal", hold_open=True)
    check(outcome.returncode != 0
          and ("engine closed stdout" in outcome.stderr
               or "engine exited before quit" in outcome.stderr),
          f"engine die-early: {outcome.returncode} {outcome.stderr!r}")
    outcome = run_controller(fixture.config("error-event", name="error.json",
                                            timeout=6), "normal",
                             extra_input="search\n")
    check(outcome.returncode != 0 and "engine emitted an error event: boom" in outcome.stderr,
          f"error event: {outcome.returncode} {outcome.stderr!r}")
    outcome = run_controller(fixture.config("spontaneous", name="order.json",
                                            timeout=6), "normal",
                             extra_input="", hold_open=True)
    check(outcome.returncode != 0 and "out-of-order engine event" in outcome.stderr,
          f"spontaneous: {outcome.returncode} {outcome.stderr!r}")
    outcome = run_controller(fixture.config("wrong-id", name="ids.json",
                                            timeout=6), "normal",
                             extra_input="search\n")
    check(outcome.returncode != 0
          and "out-of-order engine event id 999" in outcome.stderr
          and "pending action id 1" in outcome.stderr,
          f"wrong id: {outcome.returncode} {outcome.stderr!r}")
    failing_reference = [sys.executable, str(fixture.reference), "fail"]
    outcome = run_controller(fixture.config("hang", name="ref-fail.json", timeout=6,
                                            reference_argv=failing_reference), "normal",
                             extra_input="search\n")
    check(outcome.returncode != 0 and "reference exited with code 7" in outcome.stderr,
          f"reference failure: {outcome.returncode} {outcome.stderr!r}")
    outcome = run_controller(fixture.config("hang", name="eof.json", timeout=6), "normal",
                             extra_input="")
    check(outcome.returncode != 0 and "EOF before quit" in outcome.stderr,
          f"operator EOF: {outcome.returncode} {outcome.stderr!r}")
    outcome = run_controller(fixture.config("invalidready", name="ready.json",
                                            timeout=6), "normal")
    check("protocol version" in outcome.stderr,
          f"invalid ready: {outcome.stderr!r}")


def test_deadline_and_flood(fixture):
    # Acceptance 4: deadlines, floods and bounded queue growth; never success.
    started = time.monotonic()
    config = fixture.config("hang", name="deadline.json", timeout=2)
    probe = subprocess.Popen(
        [sys.executable, str(CONTROLLER), str(config)],
        stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        text=True)
    try:
        try:  # stdin stays open; only the session deadline may end the run
            probe.wait(timeout=8)
        except subprocess.TimeoutExpired:
            probe.kill()
            raise AssertionError("controller ignored its session deadline")
        stdout, stderr = probe.communicate(timeout=10)
        check(probe.returncode != 0 and "session deadline exceeded" in stderr,
              f"deadline run: {probe.returncode} {stderr!r}")
        check(time.monotonic() - started < WALL_LIMIT, "deadline test over budget")
    finally:
        if probe.poll() is None:
            probe.kill()
            probe.wait(timeout=5)
    outcome = run_controller(fixture.config("flood", name="flood.json", timeout=6), "normal",
                             extra_input="", hold_open=True)
    check(outcome.returncode != 0 and "bounded" in outcome.stderr,
          f"flood: {outcome.returncode} {outcome.stderr!r}")
    outcome = run_controller(fixture.config("overlong", name="overlong.json",
                                            timeout=6), "normal", extra_input="", hold_open=True)
    check(outcome.returncode != 0
          and "engine line" in outcome.stderr and "bound" in outcome.stderr,
          f"overlong line: {outcome.returncode} {outcome.stderr!r}")
    outcome = run_controller(fixture.config("hang", name="operator-flood.json"), "normal",
                             extra_input="x" * 100000)
    check(outcome.returncode != 0 and "operator" in outcome.stderr and "bound" in outcome.stderr,
          f"operator line bound: {outcome.returncode} {outcome.stderr!r}")


def test_strict_events(fixture):
    for scenario, diagnostic in (("bad-position", "engine complete row"),
                                 ("bad-state", "engine complete state"),
                                 ("bad-dispatch", "engine dispatch target_offset"),
                                 ("bad-menu", "engine menu_id"),
                                 ("bad-quit", "successfully acknowledge quit")):
        outcome = run_controller(fixture.config(scenario), "normal", extra_input="quit\n")
        check(outcome.returncode != 0 and diagnostic in outcome.stderr,
              f"{scenario}: {outcome.returncode} {outcome.stderr!r}")


def test_action_checkpoint(fixture):
    config = fixture.config("hang", timeout=6, checkpointSeconds=1)
    outcome = run_controller(config, "normal", extra_input="search\n", hold_open=True)
    check(outcome.returncode != 0 and "no event within the checkpoint window" in outcome.stderr,
          f"action checkpoint: {outcome.returncode} {outcome.stderr!r}")


def test_partial_startup_cleanup(fixture):
    config = fixture.config("spawn-then-die", reference_argv=[str(fixture.root / "missing.exe")])
    outcome = run_controller(config, "normal", hold_open=True)
    check(outcome.returncode != 0 and "cannot launch" in outcome.stderr,
          f"partial startup: {outcome.returncode} {outcome.stderr!r}")
    # Lock release is proven by another successful session after failed startup.
    outcome = run_controller(fixture.config("rich"), "normal", extra_input="quit\n")
    check(outcome.returncode == 0, f"restart after failed startup: {outcome.stderr!r}")


def pid_alive(pid):
    if os.name == "nt":
        kernel32 = ctypes.windll.kernel32
        kernel32.OpenProcess.restype = ctypes.c_void_p
        kernel32.OpenProcess.argtypes = [ctypes.c_ulong, ctypes.c_int, ctypes.c_ulong]
        kernel32.GetExitCodeProcess.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_ulong)]
        kernel32.CloseHandle.argtypes = [ctypes.c_void_p]
        handle = kernel32.OpenProcess(0x1000, False, pid)
        if not handle:
            return False
        try:
            exit_code = ctypes.c_ulong()
            if not kernel32.GetExitCodeProcess(handle, ctypes.byref(exit_code)):
                return False
            return exit_code.value == 259  # STILL_ACTIVE
        finally:
            kernel32.CloseHandle(handle)
    if sys.platform.startswith("linux"):
        try:
            if Path(f"/proc/{pid}/stat").read_text().rsplit(")", 1)[1].split()[0] == "Z":
                return False
        except FileNotFoundError:
            return False
    try:
        os.kill(pid, 0)
        return True
    except ProcessLookupError:
        return False
    except PermissionError:
        return True


def test_owned_tree_cleanup(fixture):
    # Acceptance 5: owned descendants die with the session; an unrelated
    # sentinel process survives controller cleanup, including the case where
    # the owned leader already exited on its own.
    sentinel = subprocess.Popen(
        [sys.executable, "-c", "import time; time.sleep(600)"])
    try:
        check(pid_alive(sentinel.pid), "sentinel not alive before the run")
        config = fixture.config("spawn-then-die", name="tree.json", timeout=3)
        outcome = run_controller(config, "normal", extra_input="", hold_open=True)
        check(outcome.returncode != 0, f"spawn-then-die should fail: {outcome.stderr!r}")
        grand_child = int(fixture.pid_file.read_text(encoding="ascii"))
        deadline = time.monotonic() + 10.0
        while pid_alive(grand_child) and time.monotonic() < deadline:
            time.sleep(0.2)
        check(not pid_alive(grand_child),
              "owned grandchild survived controller cleanup")
        check(pid_alive(sentinel.pid),
              "controller cleanup killed the unrelated sentinel process")
    finally:
        if sentinel.poll() is None:
            sentinel.terminate()
            sentinel.wait(timeout=10)


def hold_lock(lock_path):
    fd = os.open(str(lock_path), os.O_RDWR | os.O_CREAT, 0o600)
    if os.name == "nt":
        import msvcrt
        msvcrt.locking(fd, msvcrt.LK_NBLCK, 1)
    else:
        import fcntl
        fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
    return fd


def test_lock_contention(fixture):
    # Acceptance 6: busy lock rejects with a dedicated diagnostic; release
    # restores ordinary operation (the fake engine will fail on operator EOF,
    # which proves the lock stage was passed).
    lock_path = fixture.private / "goldbox-compare.lock"
    fd = hold_lock(lock_path)
    try:
        outcome = run_controller(fixture.config("hang", name="lock-busy.json",
                                                timeout=3), "normal", extra_input="")
        check(outcome.returncode != 0
              and "session lock" in outcome.stderr,
              f"contention run: {outcome.returncode} {outcome.stderr!r}")
    finally:
        if os.name == "nt":
            import msvcrt
            try:
                msvcrt.locking(fd, msvcrt.LK_UNLCK, 1)
            except OSError:
                pass
        else:
            import fcntl
            fcntl.flock(fd, fcntl.LOCK_UN)
        os.close(fd)
    outcome = run_controller(fixture.config("hang", name="lock-free.json",
                                            timeout=3), "normal", extra_input="")
    check(outcome.returncode != 0
          and "session lock" not in outcome.stderr,
          f"released run: {outcome.returncode} {outcome.stderr!r}")


def test_paths_with_spaces(fixture):
    # Acceptance 7: spaced and metacharacter paths survive without shell
    # interpretation; the fake engine records its exact cwd and argv.
    spaced_engine_dir = fixture.root / "engine & spaced (1) dir"
    spaced_engine_dir.mkdir()
    spaced_private = fixture.root / "private (2) & dir"
    spaced_private.mkdir()
    spaced_engine = spaced_engine_dir / "fake engine.py"
    spaced_engine.write_text(ENGINE_SOURCE, encoding="utf-8")
    spaced_reference = fixture.root / "fake & reference.py"
    spaced_reference.write_text(REFERENCE_SOURCE, encoding="utf-8")
    record = spaced_engine_dir / "engine-record.txt"
    config = fixture.root / "printed-paths.json"
    settings = {
        "schemaVersion": 1,
        "privateRoot": str(spaced_private),
        "engine": {"command": [sys.executable, str(spaced_engine), "writer",
                               str(record)],
                   "cwd": str(spaced_engine_dir)},
        "reference": {"command": [sys.executable, str(spaced_reference)],
                      "cwd": str(spaced_private)},
        "timeoutSeconds": 8,
        "checkpointSeconds": 2,
    }
    config.write_text(json.dumps(settings), encoding="utf-8")
    outcome = run_controller(config, "normal", extra_input="quit\n")
    check(outcome.returncode == 0, f"spaced paths: {outcome.returncode} {outcome.stderr!r}")
    check("no comparison result" in outcome.stdout, "inconclusive disclaimer missing")
    recorded = record.read_text(encoding="utf-8").splitlines()
    check(recorded and os.path.normcase(recorded[0]) == os.path.normcase(str(spaced_engine_dir)),
          f"engine cwd misapplied: {recorded[:2]!r}")
    arguments = json.loads(recorded[1]) if len(recorded) > 1 else []
    check(arguments == [str(record)],
          f"engine argv mangled by shell handling: {arguments!r}")


def test_no_writes_no_match_claim(fixture):
    # Acceptance 8: the controller never rewrites its input config or the
    # reference fixture, and its output never claims a parity MATCH.
    config_path = fixture.config("rich", name="immutable.json", timeout=8)
    config_bytes = config_path.read_bytes()
    engine_source_bytes = fixture.engine.read_bytes()
    reference_source_bytes = fixture.reference.read_bytes()
    marker_bytes = fixture.fixture_marker.read_bytes()
    outcome = run_controller(config_path, "normal",
                             extra_input="search\nquit\n")
    check(outcome.returncode == 0, f"immutable run: {outcome.returncode} {outcome.stderr!r}")
    check(config_path.read_bytes() == config_bytes, "session config was rewritten")
    check(fixture.engine.read_bytes() == engine_source_bytes, "engine fixture rewritten")
    check(fixture.reference.read_bytes() == reference_source_bytes,
          "reference fixture rewritten")
    check(fixture.fixture_marker.read_bytes() == marker_bytes, "fixture marker rewritten")
    written = sorted(entry.name for entry in fixture.private.iterdir())
    check(written == ["goldbox-compare.lock"],
          f"unexpected private-root writes: {written}")
    flow = (outcome.stdout + outcome.stderr).lower()
    check("no comparison result" in flow, "inconclusive disclaimer missing")
    check("match" not in flow, f"parity MATCH claim leaked: match positions "
          f"{[start for start in range(len(flow)) if flow.startswith('match', start)]}")


def test_compiled_engine(fixture, executable, profile):
    from harness_test import dax

    geo = bytearray(1026)
    geo[515] = 0x81
    geo_path = fixture.private / "GEO.DAX"
    ecl_path = fixture.private / "ECL.DAX"
    save_path = fixture.private / "compiled-state.json"
    geo_path.write_bytes(dax(bytes(geo)))
    ecl_path.write_bytes(dax(bytes(
        [1, 1, 20, 0x99] * 5 + [9, 1, 0x4f, 0xc0, 1, 0, 0x4a, 0x11, 0x80, 1, 4, 0])))
    config = fixture.config("rich", timeout=8, engine={
        "command": [executable, "--harness", profile, str(geo_path), "7",
                    str(ecl_path), "7", str(save_path)], "cwd": str(fixture.private)})
    outcome = run_controller(config, "normal", extra_input="east\nnorth\nsave\nquit\n")
    check(outcome.returncode == 0, f"compiled engine: {outcome.stderr!r}")
    check("[complete id=2] status=blocked" in outcome.stdout, outcome.stdout)
    check("\nA\n" in outcome.stdout and "steps=4" in outcome.stdout
          and "no comparison result" in outcome.stdout, outcome.stdout)
    state = json.loads(save_path.read_text())
    check(state["memory"][0x4a00] == 0x81, "compiled event state was not persisted")


def test_launcher_and_containment_bypasses(fixture):
    # Counterexamples: a bare bash.exe resolves to Git's bash for which() but to
    # System32's WSL redirector for CreateProcess; path aliases and ancestors of
    # the repository must not pass the "outside the repository" rule.
    sys.path.insert(0, str(CONTROLLER.parent))
    import compare_game
    for name in ("wsl.exe", "WSL", "bash", "bash.exe", "BASH.EXE", "bash.exe ",
                 r"C:\Windows\System32\bash.exe", "C:/Windows/Sysnative/wsl.exe",
                 "ubuntu.exe", "Ubuntu2204.exe", "debian.exe"):
        check(compare_game.is_wsl_launcher_name(name), f"launcher not recognized: {name!r}")
    for name in ("python.exe", "goldbox_explore.exe", "DOSBox.exe", "/usr/bin/python3"):
        check(not compare_game.is_wsl_launcher_name(name), f"false launcher match: {name!r}")
    marker = fixture.engine_dir / "fixture-marker.txt"
    if os.name == "nt":
        for launcher in ("bash.exe", "bash", "BASH.EXE"):
            config = fixture.config("hang", name="bare-bash.json",
                                    engine={"command": [launcher, "-c", "uname -sr"],
                                            "cwd": str(fixture.engine_dir)})
            outcome = run_controller(config, "normal")
            check(outcome.returncode == 1 and "unsupported backend" in outcome.stderr
                  and "Linux" not in outcome.stdout,
                  f"bare {launcher}: {outcome.returncode} {outcome.stdout!r} {outcome.stderr!r}")
        probe = subprocess.Popen([sys.executable, "-c", "import time; time.sleep(30)"])
        try:
            image = compare_game._suspended_image_path(probe)
            check(image is not None and os.path.samefile(image, sys.executable),
                  f"image path of a live child: {image!r}")
        finally:
            probe.kill()
            probe.wait(timeout=5)
    repository = CONTROLLER.parent.parent.resolve()
    aliases = [repository.parent, Path(repository.anchor)]
    if os.name == "nt":
        drive, tail = os.path.splitdrive(str(repository / "scripts"))
        aliases.append("\\\\?\\" + drive + tail)
        unc = "\\\\localhost\\" + drive[0] + "$" + tail
        if os.path.isdir(unc):
            aliases.append(unc)
    for alias in aliases:
        config = fixture.config("hang", name="alias.json", private=alias)
        outcome = run_controller(config, "normal")
        check(outcome.returncode == 1 and "outside the repository" in outcome.stderr,
              f"privateRoot {alias!s} accepted: {outcome.returncode} {outcome.stderr!r}")
    check(marker.read_bytes() == b"synthetic fixture only\n", "fixture modified")


def test_input_hardening(fixture):
    # F9: NUL, duplicate keys, non-regular/oversized config, facing range.
    # F15: engine text cannot inject terminal escapes. F16: a rejected event
    # prints nothing first.
    sys.path.insert(0, str(CONTROLLER.parent))
    import compare_game
    from compare_config import CONFIG_BYTE_LIMIT
    import contextlib
    import io
    base = json.loads(fixture.config("hang", name="base.json").read_text(encoding="utf-8"))
    cases = {
        "nul-engine-argv": lambda c: c["engine"].update(command=["a\0b"]),
        "nul-reference-argv": lambda c: c["reference"].update(command=["a\0b"]),
        "nul-private-root": lambda c: c.update(privateRoot=c["privateRoot"] + "\0x"),
    }
    for label, mutate in cases.items():
        config = json.loads(json.dumps(base))
        mutate(config)
        path = fixture.root / f"{label}.json"
        path.write_text(json.dumps(config), encoding="utf-8")
        try:
            compare_game.parse_config(path)
            check(False, f"{label}: accepted")
        except compare_game.ControlledError:
            pass
    duplicate = fixture.root / "duplicate.json"
    text = fixture.config("hang", name="dup-src.json").read_text(encoding="utf-8")
    duplicate.write_text(text[:-1] + ', "timeoutSeconds": 5}', encoding="utf-8")
    for path, label in ((duplicate, "duplicate keys"), (fixture.root, "directory"),
                        (fixture.root / "missing.json", "missing file")):
        try:
            compare_game.parse_config(path)
            check(False, f"{label}: accepted")
        except compare_game.ControlledError:
            pass
    big = fixture.root / "big.json"
    big.write_text(" " * (CONFIG_BYTE_LIMIT + 1), encoding="utf-8")
    try:
        compare_game.parse_config(big)
        check(False, "oversized configuration accepted")
    except compare_game.ControlledError:
        pass
    ready = {"type": "ready", "protocol": 1, "baseline": "unverified-empty",
             "source_identity": "x", "row": 0, "column": 0, "facing": 4}
    try:
        compare_game._render_ready(ready)
        check(False, "facing 4 accepted")
    except compare_game.ControlledError:
        pass
    sink = io.StringIO()
    with contextlib.redirect_stdout(sink):
        compare_game._render_text({"text": "a\x1b[31mred\x07" + "z" * 9000, "clear": False})
    printed = sink.getvalue()
    check("\x1b" not in printed and "\x07" not in printed, "control characters printed")
    check(len(printed) < 5000 and "[truncated]" in printed, "long field not bounded")
    sink = io.StringIO()
    with contextlib.redirect_stdout(sink):
        try:
            compare_game._render_complete(
                {"id": 1, "status": "ok", "state": {}, "row": 0, "column": 0, "facing": 0,
                 "dispatches": [], "diagnostic": 7, "steps": 0, "bytecode_offset": 0})
            check(False, "bad diagnostic accepted")
        except compare_game.ControlledError:
            pass
    check(sink.getvalue() == "", f"rejected event was partly printed: {sink.getvalue()!r}")


def test_reader_errors_are_failures(fixture):
    # F18: a pipe read error must surface as a failure, not as a silent EOF.
    sys.path.insert(0, str(CONTROLLER.parent))
    import compare_game

    class BrokenStream:
        def read1(self, size):
            raise OSError("synthetic pipe error")

    drain = compare_game._BoundedDrain(BrokenStream(), "synthetic pipe", 1024)
    drain.run()
    check(drain.failure is not None and "synthetic pipe error" in str(drain.failure),
          f"drain swallowed the read error: {drain.failure!r}")
    events = compare_game._EventQueue()
    compare_game.EngineStdoutReader(BrokenStream(), events).run()
    check(events.failure is not None and "synthetic pipe error" in str(events.failure),
          f"reader swallowed the read error: {events.failure!r}")


def test_lock_and_launch_edges(fixture):
    # F12: contention reports contention. F8: an interrupt after spawn kills the
    # child. F25: job counts come from accounting, not a 256-entry list.
    # F19/F20: WSL proxy failure paths close the real pipe, writer joined first.
    sys.path.insert(0, str(CONTROLLER.parent))
    import compare_game
    holder = compare_game.SessionLock(fixture.private).acquire()
    try:
        try:
            compare_game.SessionLock(fixture.private).acquire()
            check(False, "second lock acquired")
        except compare_game.ControlledError as error:
            check("already holds" in str(error), f"wrong contention message: {error}")
    finally:
        holder.release()
    compare_game.SessionLock(fixture.private).acquire().release()

    owned = compare_game.OwnedProcess.launch(
        [sys.executable, "-c", "import time; time.sleep(60)"], fixture.private)
    try:
        if os.name == "nt":
            check(compare_game._job_process_count(owned.job) == 1, "job count != 1")
    finally:
        owned.kill_tree()
    if os.name == "nt":
        created = []
        real_popen = subprocess.Popen

        class Recording(real_popen):
            def __init__(self, *args, **kwargs):
                super().__init__(*args, **kwargs)
                created.append(self)

        import compare_process
        original = compare_process._thread_ids_of

        def interrupted(pid):
            raise KeyboardInterrupt

        subprocess.Popen = Recording
        compare_process._thread_ids_of = interrupted
        try:
            try:
                compare_game.OwnedProcess.launch(
                    [sys.executable, "-c", "import time; time.sleep(60)"], fixture.private)
                check(False, "interrupted launch returned")
            except KeyboardInterrupt:
                pass
        finally:
            subprocess.Popen = real_popen
            compare_process._thread_ids_of = original
        check(len(created) == 1 and created[0].wait(timeout=10) is not None,
              "interrupted launch left the suspended child alive")

    native = compare_game.OwnedProcess.launch(
        [sys.executable, "-c", "import sys; sys.stdin.buffer.readline()"], fixture.private)
    try:
        compare_game.WslEngine(native, {"argv": ["x" * 200000], "cwd": "/"}, 5)
        check(False, "oversized start frame accepted")
    except compare_game.ControlledError:
        pass
    native.kill_tree()
    check(native.process.stdin.closed, "real stdin pipe leaked after __init__ failure")

    native = compare_game.OwnedProcess.launch(
        [sys.executable, "-c", "import sys; sys.stdin.buffer.readline()"], fixture.private)
    engine = compare_game.WslEngine(native, {"argv": ["x"], "cwd": "/"}, 5)
    try:
        engine.kill_tree()
    except compare_game.ControlledError:
        pass  # No Linux acknowledgment exists for this synthetic child.
    check(not engine.writer.is_alive() and engine.pipe.closed,
          "writer still alive or pipe open after kill_tree")


def test_termination_signals(fixture):
    # F5: a termination signal must run the cleanup path, not skip it. Windows
    # delivers CTRL_BREAK_EVENT; POSIX delivers SIGTERM and SIGHUP.
    sys.path.insert(0, str(CONTROLLER.parent))
    import compare_game
    if os.name == "nt":
        deliveries = [("CTRL_BREAK", lambda p: p.send_signal(signal.CTRL_BREAK_EVENT),
                       subprocess.CREATE_NEW_PROCESS_GROUP)]
    else:
        deliveries = [("SIGTERM", lambda p: p.send_signal(signal.SIGTERM), 0),
                      ("SIGHUP", lambda p: p.send_signal(signal.SIGHUP), 0)]
    for label, deliver, flags in deliveries:
        config = fixture.config("hang", name=f"signal-{label}.json", timeout=30)
        process = subprocess.Popen(
            [sys.executable, "-u", str(CONTROLLER), str(config)], stdin=subprocess.PIPE,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, creationflags=flags)
        try:
            deadline = time.monotonic() + 15
            ready = False
            while time.monotonic() < deadline and not ready:
                ready = "[engine ready]" in process.stdout.readline()
            check(ready, f"{label}: controller never reached ready")
            deliver(process)
            try:
                code = process.wait(timeout=15)
            except subprocess.TimeoutExpired:
                code = None
            check(code == 1, f"{label}: exit status {code!r}")
            check("cancelled by operator" in process.stderr.read(),
                  f"{label}: cleanup path not taken")
            compare_game.SessionLock(fixture.private).acquire().release()
        finally:
            if process.poll() is None:
                process.kill()
                process.wait(timeout=5)


def test_posix_lock_outlives_killed_controller(fixture):
    # F-A3: SIGKILL (or OOM) gives the controller no cleanup. The children run in
    # their own sessions, so the lock must stay held by them until they die.
    # SIGQUIT must take the normal cleanup path.
    if os.name == "nt":
        print("test_posix_lock_outlives_killed_controller: POSIX only; skipped")
        return
    sys.path.insert(0, str(CONTROLLER.parent))
    import compare_game
    lock_path = os.path.realpath(fixture.private / "goldbox-compare.lock")

    def holders():
        found = []
        for entry in os.listdir("/proc"):
            if not entry.isdecimal() or int(entry) == os.getpid():
                continue
            try:
                for fd in os.listdir(f"/proc/{entry}/fd"):
                    if os.path.realpath(f"/proc/{entry}/fd/{fd}") == lock_path:
                        found.append(int(entry))
                        break
            except OSError:
                continue
        return found

    for label, number, expect_clean in (("SIGQUIT", signal.SIGQUIT, True),
                                        ("SIGKILL", signal.SIGKILL, False)):
        config = fixture.config("hang", name=f"lock-{label}.json", timeout=30)
        process = subprocess.Popen(
            [sys.executable, "-u", str(CONTROLLER), str(config)], stdin=subprocess.PIPE,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        survivors = []
        try:
            deadline = time.monotonic() + 15
            ready = False
            while time.monotonic() < deadline and not ready:
                ready = "[engine ready]" in process.stdout.readline()
            check(ready, f"{label}: controller never reached ready")
            process.send_signal(number)
            process.wait(timeout=15)
            survivors = [pid for pid in holders() if pid != process.pid]
            if expect_clean:
                check(not survivors, f"{label}: cleanup left lock holders {survivors}")
                compare_game.SessionLock(fixture.private).acquire().release()
            else:
                check(len(survivors) >= 2, f"{label}: children do not hold the lock: {survivors}")
                try:
                    compare_game.SessionLock(fixture.private).acquire().release()
                except compare_game.ControlledError:
                    pass
                else:
                    check(False, f"{label}: lock free while owned children still run")
        finally:
            for pid in survivors:
                try:
                    os.kill(pid, signal.SIGKILL)
                except OSError:
                    pass
            if process.poll() is None:
                process.kill()
                process.wait(timeout=5)
        deadline = time.monotonic() + 10
        while holders() and time.monotonic() < deadline:
            time.sleep(0.1)
        compare_game.SessionLock(fixture.private).acquire().release()


def test_posix_leader_not_reaped(fixture):
    # F4: poll() on a POSIX leader must not reap it; a reaped leader frees the
    # pgid for reuse before killpg runs. Only meaningful on POSIX.
    if os.name == "nt":
        print("test_posix_leader_not_reaped: not testable on Windows; skipped")
        return
    sys.path.insert(0, str(CONTROLLER.parent))
    import compare_game
    owned = compare_game.OwnedProcess.launch([sys.executable, "-c", "pass"], fixture.private)
    deadline = time.monotonic() + 10
    while owned.process.poll() is None and time.monotonic() < deadline:
        time.sleep(0.05)
    check(owned.process.poll() == 0, "leader exit not observed")
    check(os.path.exists(f"/proc/{owned.process.pid}") or sys.platform != "linux",
          "leader was reaped by poll()")
    os.killpg(owned.posix_group, 0)  # group still exists: pgid not reusable yet
    owned.kill_tree()
    try:
        os.waitid(os.P_PID, owned.process.pid, os.WEXITED | os.WNOHANG | os.WNOWAIT)
        reaped = False
    except ChildProcessError:
        reaped = True
    check(reaped, "kill_tree did not reap the leader")


def test_interrupted_kill_is_retried(fixture):
    # N1: an interrupt that lands before the kill takes effect must be retried for
    # real (the old retry returned early because `killed` was set on entry), and a
    # kill that never completed must keep the session lock held.
    sys.path.insert(0, str(CONTROLLER.parent))
    import compare_game
    import compare_process
    seen = []
    original_kill = compare_game.OwnedProcess.kill_tree

    def recording(self):
        if self not in seen:
            seen.append(self)
        original_kill(self)

    class Gate:
        """Raises KeyboardInterrupt at the first (or every) kill syscall."""
        always = False
        hits = 0

    gate = Gate()
    if os.name == "nt":
        real_kernel = compare_process._KERNEL32

        class Proxy:
            def __getattr__(self, name):
                return getattr(real_kernel, name)

            def TerminateJobObject(self, *args):
                if gate.always or gate.hits == 0:
                    gate.hits += 1
                    raise KeyboardInterrupt
                return real_kernel.TerminateJobObject(*args)

        def arm():
            compare_process._KERNEL32 = Proxy()

        def disarm():
            compare_process._KERNEL32 = real_kernel
    else:
        real_killpg = os.killpg

        def interrupting(group, sig):
            if sig != 0 and (gate.always or gate.hits == 0):
                gate.hits += 1
                raise KeyboardInterrupt
            return real_killpg(group, sig)

        def arm():
            os.killpg = interrupting

        def disarm():
            os.killpg = real_killpg

    held = []
    real_lock = compare_game.SessionLock

    class RecordingLock(real_lock):
        def acquire(self):
            result = super().acquire()
            held.append(self)
            return result

    compare_game.SessionLock = RecordingLock
    compare_game.OwnedProcess.kill_tree = recording
    try:
        for label, always in (("single", False), ("double", True)):
            del seen[:]
            gate.always, gate.hits = always, 0
            config = compare_game.parse_config(
                fixture.config("hang", name=f"interrupt-{label}.json", timeout=2))
            arm()
            message = ""
            try:
                compare_game.run_session(config)
            except compare_game.ControlledError as error:
                message = str(error)
            except KeyboardInterrupt:
                message = "KeyboardInterrupt"
            finally:
                disarm()
            if label == "single":
                check(all(owned.process.poll() is not None for owned in seen) and len(seen) == 2,
                      f"single: the retried kill did not stop every tree ({len(seen)} trees)")
                real_lock(config["privateRoot"]).acquire().release()
            else:
                check("interrupted twice" in message, f"double: unexpected message {message!r}")
                try:
                    real_lock(config["privateRoot"]).acquire().release()
                except compare_game.ControlledError:
                    pass
                else:
                    check(False, "double: lock released while a kill never completed")
                for owned in seen:  # Clean up what the injected interrupts left alive.
                    original_kill(owned)
                held[-1].release()
    finally:
        disarm()
        compare_game.OwnedProcess.kill_tree = original_kill
        compare_game.SessionLock = real_lock


def test_streamdown_does_not_reap_leader(fixture):
    # F-A2: waiting for the engine to exit must not reap a POSIX group leader;
    # a reaped leader frees its pgid for reuse before kill_tree's killpg.
    if os.name == "nt":
        print("test_streamdown_does_not_reap_leader: POSIX only; skipped")
        return
    import queue
    import types
    sys.path.insert(0, str(CONTROLLER.parent))
    import compare_game
    import compare_session
    owned = compare_game.OwnedProcess.launch([sys.executable, "-c", "pass"], fixture.private)
    try:
        session = object.__new__(compare_session.Session)
        session.engine = owned
        session.deadline = time.monotonic() + 10
        session._check_housekeeping = lambda: None
        session.engine_queue = types.SimpleNamespace(
            queue=queue.Queue(), reader=None, failure=None)
        session.engine_queue.queue.put(("eof",))
        session._finish_streamdown()
        try:
            os.waitid(os.P_PID, owned.process.pid, os.WEXITED | os.WNOHANG | os.WNOWAIT)
        except ChildProcessError:
            check(False, "_finish_streamdown reaped the group leader before killpg")
        os.killpg(owned.posix_group, 0)  # the pgid must still be ours
    finally:
        owned.kill_tree()


def test_reference_input_cancel(fixture):
    # F11: once the controller gives up on a dispatch, no further key may start.
    import threading
    sys.path.insert(0, str(CONTROLLER.parent))
    import reference_input
    messages = object.__new__(reference_input.WindowsMessages)
    messages.window = lambda deadline: (_ for _ in ()).throw(AssertionError("key started after cancel"))
    messages.cancel = threading.Event()
    messages.cancel.set()
    try:
        messages.send(["ENTER", "UP"], time.monotonic() + 5)
    except reference_input.ReferenceInputError as error:
        check("cancelled" in str(error), f"unexpected message: {error}")
    else:
        check(False, "send ran after cancel")


def test_cleanup_failure_handling(fixture):
    # F6: a second interrupt during kill_tree must not skip the other tree or the
    # lock release. F7: a cleanup failure must not replace the session failure.
    sys.path.insert(0, str(CONTROLLER.parent))
    import compare_game
    original = compare_game.OwnedProcess.kill_tree
    seen = []

    def interrupted_once(self):
        seen.append(self)
        if seen.count(self) == 1:
            raise KeyboardInterrupt
        original(self)

    def failing(self):
        seen.append(self)
        original(self)
        raise OSError("synthetic kill failure")

    held = []
    real_lock = compare_game.SessionLock

    class RecordingLock(real_lock):
        def acquire(self):
            result = super().acquire()
            held.append(self)
            return result

    compare_game.SessionLock = RecordingLock
    try:
        for label, patched in (("interrupt", interrupted_once), ("failure", failing)):
            del seen[:]
            compare_game.OwnedProcess.kill_tree = patched
            config = compare_game.parse_config(
                fixture.config("hang", name=f"cleanup-{label}.json", timeout=2))
            message = ""
            try:
                compare_game.run_session(config)
            except compare_game.ControlledError as error:
                message = str(error)
            # In-process the console hits EOF (or the deadline): either is the
            # session failure that must stay visible.
            check(message and not message.startswith("cleanup failed"),
                  f"{label}: session failure lost: {message!r}")
            if label == "failure":
                check("synthetic kill failure" in message,
                      f"{label}: cleanup failure not reported: {message!r}")
            check(len({id(owned) for owned in seen}) == 2,
                  f"{label}: not every tree was cleaned: {len(seen)} calls")
            check(all(owned.process.poll() is not None for owned in seen),
                  f"{label}: an owned process survived")
            if label == "failure":
                # F13: ownership is ambiguous after a failed kill, so the lock stays held.
                try:
                    compare_game.SessionLock(config["privateRoot"]).acquire().release()
                except compare_game.ControlledError:
                    pass
                else:
                    check(False, "lock was released despite a cleanup failure")
                held[-1].release()
            else:
                compare_game.SessionLock(config["privateRoot"]).acquire().release()
    finally:
        compare_game.OwnedProcess.kill_tree = original
        compare_game.SessionLock = real_lock


def main():
    with tempfile.TemporaryDirectory() as temp:
        fixture = Fixture(temp)
        test_render_flow(fixture)
        test_malformed_configs(fixture)
        test_protocol_failures(fixture)
        test_deadline_and_flood(fixture)
        test_strict_events(fixture)
        test_action_checkpoint(fixture)
        test_partial_startup_cleanup(fixture)
        test_owned_tree_cleanup(fixture)
        test_lock_contention(fixture)
        test_paths_with_spaces(fixture)
        test_no_writes_no_match_claim(fixture)
        test_launcher_and_containment_bypasses(fixture)
        test_cleanup_failure_handling(fixture)
        test_input_hardening(fixture)
        test_lock_and_launch_edges(fixture)
        test_reader_errors_are_failures(fixture)
        test_termination_signals(fixture)
        test_posix_leader_not_reaped(fixture)
        test_posix_lock_outlives_killed_controller(fixture)
        test_interrupted_kill_is_retried(fixture)
        test_streamdown_does_not_reap_leader(fixture)
        test_reference_input_cancel(fixture)
        if len(sys.argv) == 3:
            test_compiled_engine(fixture, sys.argv[1], sys.argv[2])
    print("compare_game tests passed")


if __name__ == "__main__":
    main()
