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
        if len(sys.argv) == 3:
            test_compiled_engine(fixture, sys.argv[1], sys.argv[2])
    print("compare_game tests passed")


if __name__ == "__main__":
    main()
