# SPDX-License-Identifier: GPL-3.0-or-later
"""Bounded Pool v1.3 startup checkpoint adapter over the existing DOS harness.

Compares position, facing, three opening flags and the interpreter return PC.
This is deliberately a partial parity check: party rolls, pixels, sound, timing,
all memory and the original save format are outside its comparison coverage.
All captures and copied original resources stay in the supplied private folder.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import tempfile
import time

from compare_lock import SessionLock
from compare_process import OwnedProcess
from reference_input import WindowsMessages
from rng_reference import PoolDosRandom, WindowsMemory

DOSBOX_SHA256 = "dcfd46fa521f5ce89dce3bf026056f3a1d15533f80321ee887403e30d7949f5e"
OVERLAY_SHA256 = "872964a32cef0fc32bbf83bab68b15e04ad36d59076f37b00a179dfee7928614"
FLAGS = (0x4a17, 0x4ac4, 0x4ac5)
# Identical native 320x200 program-menu PNG in three isolated installed-release runs.
MENU_CAPTURE_SHA256 = "77c4a89275e3e96c0dc37481cb638cb63c1c2c8c93b41e18c1c85881844e2c3a"
CREATION_MENUS = (
    "4c415feacf03dfec426b274a3bd4248ff892945eb1e85415e743b3dd53d744fd",
    "89d28cf5ac939259134d4ed39d527adf32363f57c19642bc88aceb618ccda324",
    "f945370e0deac698f54ffd9cfb8a98826f1a8dc4d026c18972f57a5c107112da",
    "13ed24a51574a43f5fb9b9ef0c41841da93bf66921b76e9980ad5bc092a1a591",
)


def read_checkpoint(path):
    if path.stat().st_size > 4096:
        raise ValueError("Engine checkpoint exceeds 4096 bytes")
    value = json.loads(path.read_text(encoding="utf-8-sig"))
    if not isinstance(value, dict):
        raise ValueError("Engine checkpoint must be an object")
    bounds = {"column": 15, "row": 15, "facing": 3, "bytecodeOffset": 65534, "codeBase": 65535}
    for key, maximum in bounds.items():
        if type(value.get(key)) is not int or not 0 <= value[key] <= maximum:
            raise ValueError("Invalid engine checkpoint: " + key)
    if (value.get("status") != "original-resource-replay" or type(value.get("programRecord")) is not int
            or value["programRecord"] != 0
            or value.get("saveRoundTrip") is not True or value.get("deterministicReplay") is not True
            or value["codeBase"] + value["bytecodeOffset"] + 1 > 65535):
        raise ValueError("Expected a completed program-zero engine startup check")
    if set(value.get("flags", {})) != {str(address) for address in FLAGS}:
        raise ValueError("Missing opening flags")
    if any(type(n) is not int or not 0 <= n <= 255 for n in value["flags"].values()):
        raise ValueError("Invalid opening flag value")
    return value


def matches(expected, observed):
    return (all(observed[key] == expected[key] for key in ("column", "row", "facing", "flags"))
            and observed["programCounter"] == expected["codeBase"] + expected["bytecodeOffset"] + 1)


def movement_flags(data, read, guest_base):
    """Read the fingerprinted build's separate, word-backed ECL globals."""
    if len(data) != 65536:
        raise ValueError("Incomplete native data segment")
    offset = int.from_bytes(data[0x49d6:0x49d8], "little")
    segment = int.from_bytes(data[0x49d8:0x49da], "little")
    if not segment:  # Not allocated before the original has initialized ECL.
        return {}
    last = offset + 2 * (0x6dd5 - 0x6b00) + 2
    if last > 65536 or segment * 16 + last > 0x100000:
        raise ValueError("Native movement flags outside guest memory")
    flags = {}
    for address in (0x6dc9, 0x6dd5):
        raw = read(guest_base + segment * 16 + offset + 2 * (address - 0x6b00), 2)
        if len(raw) != 2:
            raise RuntimeError("Incomplete native movement flag snapshot")
        flags[str(address)] = int.from_bytes(raw, "little")
    return flags


def verify(path, expected):
    if (path.is_symlink() or not path.is_file() or path.stat().st_size > 8 * 1024 * 1024
            or hashlib.sha256(path.read_bytes()).hexdigest() != expected):
        raise ValueError("Unsupported original resource fingerprint: " + path.name)


def run(installation, private_parent, resident_image, checkpoint):
    if os.name != "nt":
        raise RuntimeError("The live reference adapter requires native Windows")
    repository = Path(__file__).resolve().parents[1]
    private_parent = private_parent.resolve(strict=True)
    if private_parent.is_relative_to(repository) or not private_parent.is_dir():
        raise ValueError("Private captures must be outside the project repository")
    expected = read_checkpoint(checkpoint)
    adapter = PoolDosRandom(resident_image)
    installation = installation.resolve(strict=True)
    if private_parent.is_relative_to(installation):
        raise ValueError("Private copies must stay outside the original installation")
    game = installation / "GAME" / "POOLRAD"
    verify(game / "START.EXE", adapter.start_sha256)
    verify(game / "GAME.OVR", OVERLAY_SHA256)
    verify(installation / "DOSBOX" / "DOSBox.exe", DOSBOX_SHA256)
    files = []
    total = 0
    for path in game.rglob("*"):
        if path.is_symlink() or not path.resolve().is_relative_to(game.resolve()):
            raise ValueError("Installation contains linked or escaping paths")
        total += path.stat().st_size if path.is_file() else 0
        if len(files) >= 512 or total > 32 * 1024 * 1024:
            raise ValueError("Installation copy exceeds limits")
        files.append(path)
    root = Path(tempfile.mkdtemp(prefix="startup-reference-", dir=private_parent))
    for path in files:
        if path.is_file():
            target = root / "GAME" / "POOLRAD" / path.relative_to(game)
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(path, target)
    for name in ("DOSBox.exe", "SDL.dll", "SDL_net.dll"):
        source = installation / "DOSBOX" / name
        if (source.is_symlink() or not source.is_file() or source.stat().st_size > 8 * 1024 * 1024
                or not source.resolve().is_relative_to(installation)):
            raise ValueError("Missing or linked DOSBox runtime")
        shutil.copyfile(source, root / name)
    captures = root / "captures"
    captures.mkdir()
    mapper = root / "mapper.map"
    bindings = ['hand_scrshot "key 122"', 'key_enter "key 13"', 'key_esc "key 27"', 'key_space "key 32"']
    bindings += [f'key_{chr(n)} "key {n}"' for n in range(97, 122)]
    bindings += [f'key_{c} "key {ord(c)}"' for c in "0123456789"]
    bindings += [f'key_kp_{n} "key {256+n}"' for n in range(10)]
    mapper.write_text("\n".join(bindings) + "\n", encoding="ascii")
    config = root / "reference.conf"
    config.write_text(f'[sdl]\nfullscreen=false\nautolock=false\noutput=surface\nusescancodes=false\n'
                      f'mapperfile={mapper}\n[dosbox]\nmemsize=16\ncaptures={captures}\n'
                      f'[cpu]\ncore=normal\ncycles=fixed 10000\n[speaker]\ntandy=on\npcspeaker=true\n'
                      f'[mixer]\nnosound=true\n[autoexec]\n@echo off\nmount c "{root / "GAME"}"\n'
                      'c:\ncd poolrad\nstart STING\n', encoding="ascii")
    lock = SessionLock(root).acquire()
    owned = None
    observations = []
    matched = False
    try:
        owned = OwnedProcess.launch([str(root / "DOSBox.exe"), "-conf", str(config), "-noconsole"], root,
                                    env=dict(os.environ, SDL_VIDEODRIVER="windib"))
        deadline = time.monotonic() + 120
        reader = WindowsMemory(owned.process)
        candidates = []
        while time.monotonic() < deadline - 105 and not candidates:
            candidates = reader.find(adapter, min(deadline, time.monotonic() + 5))
            time.sleep(0.1)
        if len(candidates) != 1:
            raise RuntimeError("Expected one validated original runtime image")
        image_base = candidates[0][0] - 0x10be8
        system = int.from_bytes(reader.read(image_base + 9, 2), "little")
        guest_base = image_base - (system - 0xaf8) * 16
        sender = WindowsMessages(owned.process, {"backend": "windows-messages", "actions": {}, "menus": {}})

        def observe(label):
            before = set(captures.glob("*.png"))
            sender.send(["Z"], deadline)
            capture_deadline = min(deadline, time.monotonic() + 2)
            fresh = []
            while time.monotonic() < capture_deadline and not fresh:
                fresh = list(set(captures.glob("*.png")) - before)
                time.sleep(0.02)
            if len(fresh) != 1:
                raise RuntimeError("Expected one fresh DOSBox capture")
            time.sleep(0.1)
            shutil.copyfile(fresh[0], root / (label + ".png"))
            data = reader.read(image_base + 0xc7c0, 65536)
            if len(data) != 65536 or data[0x6aaf] not in (0, 2, 4, 6):
                raise RuntimeError("Invalid native runtime position snapshot")
            offset = int.from_bytes(data[0x49d2:0x49d4], "little")
            segment = int.from_bytes(data[0x49d4:0x49d6], "little")
            flags = {}
            if segment:
                for address in FLAGS:
                    raw = reader.read(guest_base + segment * 16 + offset + 2 * (address - 0x4900), 2)
                    if len(raw) != 2:
                        raise RuntimeError("Incomplete native flag snapshot")
                    flags[str(address)] = int.from_bytes(raw, "little")
            observation = dict(label=label, column=data[0x6aad], row=data[0x6aae], facing=data[0x6aaf] // 2,
                               flags=flags, programCounter=int.from_bytes(data[0x49ed:0x49ef], "little"),
                               movementFlags=movement_flags(data, reader.read, guest_base),
                               captureSha256=hashlib.sha256(fresh[0].read_bytes()).hexdigest())
            observations.append(observation)
            return observation

        # Select the first race/class/alignment. Authored name, no game text in this script.
        # Keep both portraits, accept the icon, add the new character, then Begin.
        for index in range(20):
            if observe(f"ready-{index:02}")["captureSha256"] == MENU_CAPTURE_SHA256:
                break
            sender.send(["ENTER"], deadline)
            time.sleep(0.5)
        else:
            raise RuntimeError("Original program menu was not recognized; refusing setup input")
        time.sleep(1)
        sequence = [["C"], ["ENTER"], ["ENTER"], ["ENTER"], ["ENTER"], ["Y"],
                    ["T", "E", "S", "T", "ENTER"], ["K"], ["E"], ["Y"], ["Y"],
                    ["A"], ["ENTER"], ["E"], ["B"]]
        for index, keys in enumerate(sequence):
            sender.send(keys, deadline)
            time.sleep(0.7)
            observation = observe(f"setup-{index:02}")
            if index < len(CREATION_MENUS):
                for attempt in range(12):
                    if observation["captureSha256"] == CREATION_MENUS[index]:
                        break
                    time.sleep(0.4)
                    observation = observe(f"setup-{index:02}-wait-{attempt:02}")
                else:
                    raise RuntimeError(f"Original creation stage {index} was not recognized; input stopped")
        stable = 0
        for index in range(80):
            if time.monotonic() + 3 >= deadline:
                break
            sender.send(["ENTER"], deadline)
            time.sleep(0.5)
            observation = observe(f"opening-{index:02}")
            stable = stable + 1 if matches(expected, observation) else 0
            if stable >= 2:
                matched = True
                break
    finally:
        try:
            if owned is not None:
                owned.kill_tree()
        finally:
            lock.release()
    report = dict(matched=matched, coverage=["position", "facing", "opening-flags", "return-PC"],
                  fullParityVerified=False, expected=expected, observations=observations, cleanupVerified=True,
                  privateRoot=str(root), originalInstallationModified=False)
    (root / "comparison.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("installation", type=Path)
    parser.add_argument("private_parent", type=Path)
    parser.add_argument("resident_image", type=Path)
    parser.add_argument("engine_checkpoint", type=Path)
    args = parser.parse_args()
    report = run(args.installation, args.private_parent, args.resident_image, args.engine_checkpoint)
    print(json.dumps({key: report[key] for key in ("matched", "coverage", "fullParityVerified", "privateRoot")}))
    return 0 if report["matched"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
