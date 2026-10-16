# SPDX-License-Identifier: GPL-3.0-or-later
"""Synthetic mapping tests and Windows dual-dispatch integration; no game data."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import threading
import time
from types import SimpleNamespace

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "scripts"))
from reference_input import Mapping, ReferenceInputError, key_sequence, validate_config

CONFIG = {"backend": "windows-messages", "actions": {"search": ["S"], "save": ["V"]},
          "relativeMovement": {"forward": ["UP"], "left": ["LEFT"], "right": ["RIGHT"]},
          "menus": {"1": [["A"], ["B"]]}}

WINDOW_SOURCE = r'''
import ctypes, json, sys, time
from ctypes import wintypes as w
u=ctypes.WinDLL('user32',use_last_error=True)
k=ctypes.WinDLL('kernel32',use_last_error=True)
LRESULT=ctypes.c_ssize_t
PROC=ctypes.WINFUNCTYPE(LRESULT,w.HWND,w.UINT,w.WPARAM,w.LPARAM)
class WNDCLASS(ctypes.Structure):
    _fields_=[('style',w.UINT),('proc',PROC),('class_extra',ctypes.c_int),('window_extra',ctypes.c_int),
              ('instance',w.HINSTANCE),('icon',w.HICON),('cursor',w.HANDLE),('background',w.HBRUSH),
              ('menu',w.LPCWSTR),('name',w.LPCWSTR)]
k.GetModuleHandleW.restype=w.HMODULE
k.GetModuleHandleW.argtypes=[w.LPCWSTR]
u.RegisterClassW.argtypes=[ctypes.POINTER(WNDCLASS)]
u.RegisterClassW.restype=w.WORD
u.CreateWindowExW.argtypes=[w.DWORD,w.LPCWSTR,w.LPCWSTR,w.DWORD,ctypes.c_int,ctypes.c_int,
                          ctypes.c_int,ctypes.c_int,w.HWND,w.HMENU,w.HINSTANCE,w.LPVOID]
u.CreateWindowExW.restype=w.HWND
u.DefWindowProcW.argtypes=[w.HWND,w.UINT,w.WPARAM,w.LPARAM]
u.DefWindowProcW.restype=LRESULT
u.ShowWindow.argtypes=[w.HWND,ctypes.c_int]
u.SetTimer.argtypes=[w.HWND,ctypes.c_size_t,w.UINT,w.LPVOID]
u.SetTimer.restype=ctypes.c_size_t
u.DestroyWindow.argtypes=[w.HWND]
u.GetMessageW.argtypes=[ctypes.POINTER(w.MSG),w.HWND,w.UINT,w.UINT]
u.TranslateMessage.argtypes=[ctypes.POINTER(w.MSG)]
u.DispatchMessageW.argtypes=[ctypes.POINTER(w.MSG)]
u.DispatchMessageW.restype=LRESULT
@PROC
def proc(hwnd,msg,vk,param):
    if msg in (0x100,0x101):
        if len(sys.argv)>2 and sys.argv[2]=='hang' and msg==0x100 and vk==ord('S'):
            time.sleep(1)
        with open(sys.argv[1],'a',encoding='ascii') as log:
            log.write(json.dumps({'message':msg,'vk':vk,'flags':param})+'\n')
        return 0
    if msg==0x113:
        u.DestroyWindow(hwnd)
        return 0
    if msg==2:
        u.PostQuitMessage(0)
        return 0
    return u.DefWindowProcW(hwnd,msg,vk,param)
wc=WNDCLASS()
wc.proc=proc
wc.instance=k.GetModuleHandleW(None)
wc.name='SDL_app'
assert u.RegisterClassW(ctypes.byref(wc))
hwnd=u.CreateWindowExW(0,'SDL_app','Synthetic Goldbox reference',0x00cf0000,
                      50,50,240,100,None,None,wc.instance,None)
assert hwnd
u.ShowWindow(hwnd,4)
u.SetTimer(hwnd,1,20000,None)
open(sys.argv[1]+'.ready','w').close()
message=w.MSG()
while u.GetMessageW(ctypes.byref(message),None,0,0)>0:
    u.TranslateMessage(ctypes.byref(message))
    u.DispatchMessageW(ctypes.byref(message))
'''


def rejected(action):
    try:
        action()
    except ReferenceInputError:
        return
    raise AssertionError("invalid mapping accepted")


def test_mappings():
    mapping = Mapping(CONFIG)
    for facing in range(4):
        for target, action in enumerate(("north", "east", "south", "west")):
            keys = mapping.action(action, facing)
            result = facing
            for key in keys[:-1]:
                result = (result + (1 if key == "RIGHT" else -1)) % 4
            assert result == target and keys[-1] == "UP"
    assert mapping.choice(1, 1, 2) == ["B"]
    for action in (lambda: mapping.action("load", 0), lambda: mapping.action("north", True),
                   lambda: mapping.choice(2, 0, 2), lambda: mapping.choice(1, 0, 3),
                   lambda: key_sequence([]), lambda: key_sequence(["ALT+F4"]),
                   lambda: key_sequence(["S"] * 17), lambda: validate_config(None),
                   lambda: Mapping(dict(CONFIG, actions={"north": ["UP"]})),
                   lambda: Mapping(dict(CONFIG, menus={"0": [["A"]]}))):
        rejected(action)


def test_dispatch_gate():
    from compare_game import ControlledError, Session

    engine_seen, reference_seen = threading.Event(), threading.Event()

    class EngineInput:
        def write(self, _):
            engine_seen.set()
            assert reference_seen.wait(timeout=2), "reference did not start concurrently"

        def flush(self):
            pass

    def reference_send(keys, deadline):
        reference_seen.set()
        assert engine_seen.wait(timeout=2), "engine did not start concurrently"

    session = object.__new__(Session)
    session.engine = SimpleNamespace(process=SimpleNamespace(stdin=EngineInput()))
    session.reference_input = SimpleNamespace(send=reference_send)
    session.deadline, session.config = time.monotonic() + 3, {"checkpoint": 3}
    session._check_housekeeping = lambda: None
    session._send({"id": 1, "action": "search"}, ["S"])

    def failed_worker(*_):
        raise RuntimeError("synthetic worker fault")

    session.reference_input.send = failed_worker
    try:
        session._send({"id": 2, "action": "search"}, ["S"])
    except ControlledError as error:
        assert "delivery may be partial" in str(error) and "synthetic worker fault" in str(error)
    else:
        raise AssertionError("worker exception was reported as successful dispatch")


def _windows_flow(root, backend):
    from compare_game_test import Fixture
    fixture = Fixture(root)
    window = root / "synthetic_window.py"
    window.write_text(WINDOW_SOURCE, encoding="utf-8")
    keys = fixture.private / "keys.jsonl"

    def config(mapping=CONFIG):
        extra = {"reference": {"command": [sys.executable, str(window), str(keys)],
                               "cwd": str(fixture.private), "input": mapping}}
        if backend == "wsl":
            from wsl_engine_test import linux_path
            extra["engine"] = {"backend": "wsl", "distribution": "Ubuntu",
                "cwd": linux_path(fixture.engine_dir),
                "command": ["/usr/bin/python3", linux_path(fixture.engine), "rich"]}
        return fixture.config("rich", timeout=15, checkpointSeconds=5, **extra)

    controller = [sys.executable, str(ROOT / "scripts/compare_game.py")]
    run = subprocess.run(controller + [str(config())],
        input="reference ENTER\narm\nsearch\nnorth\neast\nsearch\n1\nquit\n",
        capture_output=True, text=True, timeout=25)
    assert run.returncode == 0, (run.stdout, run.stderr)
    assert "id=4 menu=1" in run.stdout and "no comparison result" in run.stdout
    events = [json.loads(line) for line in keys.read_text().splitlines()]
    downs = [event for event in events if event["message"] == 0x100]
    ups = [event for event in events if event["message"] == 0x101]
    assert [event["vk"] for event in downs] == [13, ord('S'), 38, 39, 38, ord('S'), ord('B')], downs
    assert len(downs) == len(ups)
    assert all(event["flags"] & 0xc0000000 == 0xc0000000 for event in ups)
    assert all(event["flags"] & (1 << 24) for event in downs if event["vk"] in (38, 39))

    # Missing mappings and disarmed commands must fail before either side receives input.
    for input_text, diagnostic in (("arm\nload\n", "no reference mapping for load"),
                                   ("north\n", "dual input is disarmed")):
        before = keys.read_bytes()
        run = subprocess.run(controller + [str(config())], input="reference SPACE\n" + input_text,
                             capture_output=True, text=True, timeout=20)
        assert run.returncode != 0 and diagnostic in run.stderr, (run.stdout, run.stderr)
        assert "sent action id" not in run.stdout
        new = keys.read_bytes()[len(before):].decode()
        assert all(json.loads(line)["vk"] == 32 for line in new.splitlines()), new

    settings = json.loads(config().read_text())
    settings["reference"]["command"].append("hang")
    bad_reference = root / "hung-reference.json"
    bad_reference.write_text(json.dumps(settings))
    run = subprocess.run(controller + [str(bad_reference)],
        input="reference SPACE\narm\nsearch\neast\nquit\n", capture_output=True, text=True, timeout=20)
    assert run.returncode != 0 and "delivery may be partial" in run.stderr, (run.stdout, run.stderr)
    assert "sent action id 2" not in run.stdout, run.stdout


def test_windows_flow(root, backend="native"):
    sentinel_script = root / "sentinel_window.py"
    sentinel_script.write_text(WINDOW_SOURCE)
    sentinel_log = root / "unrelated-keys.jsonl"
    sentinel = subprocess.Popen([sys.executable, str(sentinel_script), str(sentinel_log)])
    try:
        limit = time.monotonic() + 4
        while not Path(str(sentinel_log) + ".ready").exists():
            assert sentinel.poll() is None and time.monotonic() < limit
            time.sleep(0.02)
        _windows_flow(root, backend)
        assert sentinel.poll() is None, "unrelated reference window was terminated"
        assert not sentinel_log.exists(), "input reached an unrelated SDL window"
    finally:
        if sentinel.poll() is None:
            sentinel.terminate()
        sentinel.wait(timeout=3)


def main():
    test_mappings()
    test_dispatch_gate()
    if os.name == "nt":
        with tempfile.TemporaryDirectory() as temp:
            test_windows_flow(Path(temp))
    print("reference input tests passed")


if __name__ == "__main__":
    main()
