# SPDX-License-Identifier: GPL-3.0-or-later
"""JSONL exploration harness protocol against independently authored bytes."""
import json
from pathlib import Path
import queue
import struct
import subprocess
import sys
import tempfile
import threading

TIMEOUT = 10.0


def prototype_profile(path, root):
    """Keep older protocol fixtures on their explicit move-then-event dialect."""
    source = Path(path).resolve()
    profile = json.loads(source.read_text())
    profile["eventDispatch"].pop("movementAddresses", None)
    if "characterCreationRules" in profile:
        rules = (source.parent / profile["characterCreationRules"]).resolve()
        (root / "prototype-rules.json").write_text(rules.read_text())
        profile["characterCreationRules"] = "prototype-rules.json"
    target = root / "prototype-profile.json"
    target.write_text(json.dumps(profile))
    return str(target)


def dax(raw):
    packed = b"".join(bytes([len(raw[i:i + 128]) - 1]) + raw[i:i + 128] for i in range(0, len(raw), 128))
    return struct.pack("<HBIHH", 9, 7, 0, len(raw), len(packed)) + packed


class Harness:
    """Popen wrapper: writes JSONL requests, reads flushed JSONL events."""
    active = set()

    def __init__(self, executable, profile, root, geo="GEO.DAX", ecl="ECL.DAX", save="state.json"):
        self.process = subprocess.Popen(
            [executable, "--harness", profile, str(root / geo), "7", str(root / ecl), "7",
             str(root / save)],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            text=True, bufsize=1)
        self.events = queue.Queue()
        self.active.add(self)
        def drain():
            for line in self.process.stdout:
                self.events.put(line)
            self.events.put(None)
        self.reader = threading.Thread(target=drain, daemon=True)
        self.reader.start()

    def send(self, request):
        self.process.stdin.write((request if isinstance(request, str) else json.dumps(request)) + "\n")
        self.process.stdin.flush()

    def event(self, timeout=TIMEOUT):
        item = self.events.get(timeout=timeout)
        assert item is not None, "harness closed stdout before an expected event"
        return json.loads(item)

    def finish(self, timeout=TIMEOUT):
        try:
            self.process.stdin.close()
            code = self.process.wait(timeout=timeout)
            return code, self.process.stderr.read(65536)
        finally:
            self.close()

    def close(self):
        if self.process.poll() is None:
            self.process.kill()
            self.process.wait(timeout=TIMEOUT)
        self.reader.join(timeout=TIMEOUT)
        for stream in (self.process.stdin, self.process.stdout, self.process.stderr):
            stream.close()
        self.active.discard(self)


def identity_checks(ready, row=0, column=0):
    assert ready["type"] == "ready" and ready["protocol"] == 1
    assert ready["row"] == row and ready["column"] == column and ready["facing"] == 0
    assert ready["baseline"] == "unverified-empty" and ready["source_identity"]


def check_event(text, expected_id):
    assert text["type"] == "text" and text["id"] == expected_id
    assert text["text"] == "A" and text["clear"] is False


def check_complete(document, expected_id, status, row, column, facing):
    assert document["type"] == "complete" and document["id"] == expected_id
    assert document["status"] == status
    assert document["row"] == row and document["column"] == column
    assert document["facing"] == facing
    assert isinstance(document["dispatches"], list)
    assert document["state"]["schemaVersion"] == 1 and isinstance(document["state"]["dialect"], str)
    assert len(document["state"]["memory"]) == 65536


def run_lines(command, input_text, timeout=TIMEOUT):
    finished = subprocess.run(command, input=input_text, text=True, capture_output=True, timeout=timeout)
    lines = [json.loads(line) for line in finished.stdout.splitlines() if line.strip()]
    return finished, lines


def actions(*names, crlf=False):
    ending = "\r\n" if crlf else "\n"
    return "".join(json.dumps({"id": index, "action": name}) + ending
                   for index, name in enumerate(names, 1))


def main():
    with tempfile.TemporaryDirectory() as temp:
        root = Path(temp)
        executable, profile = sys.argv[1], sys.argv[2]
        # Exercise the selected lifecycle through both real code-driven adapters.
        geo = bytearray(1026)
        geo[3] = 0x10  # North wall at (1,0).
        geo[258] = 7; geo[770] = 64  # Ordinary west-boundary door at (0,0).
        (root / "NATIVE_GEO.DAX").write_bytes(dax(geo))
        code = bytearray([1,1,20,0x99] * 5 + [9,1,0x4b,0xc0,1,0,0x4a,
            9,1,0xd5,0x6d,1,4,0x4a,0])
        code[6] = len(code)
        code.extend([9,1,0x4b,0xc0,1,1,0x4a,0])
        (root / "NATIVE_ECL.DAX").write_bytes(dax(code))
        native = [executable,"--harness",profile,str(root / "NATIVE_GEO.DAX"),"7",
                  str(root / "NATIVE_ECL.DAX"),"7",str(root / "native.json")]
        finished, lines = run_lines(native,actions("east","north","save","quit"))
        assert finished.returncode == 0, finished.stderr
        first, blocked = lines[1:3]
        assert first["state"]["memory"][0x4a00] == 0 and first["state"]["memory"][0x4a01] == 1
        assert blocked["status"] == "blocked" and blocked["steps"] > 0 and blocked["column"] == 1
        terminal = [native[0],*native[2:-1],str(root / "native-terminal.json")]
        finished = subprocess.run(terminal,input="east\nnorth\nsave\nquit\n",text=True,capture_output=True,timeout=30)
        assert finished.returncode == 0, finished.stderr
        assert json.loads((root / "native.json").read_text()) == json.loads((root / "native-terminal.json").read_text())
        finished, lines = run_lines(native,actions("west","quit"))
        assert finished.returncode == 0 and lines[1]["column"] == 15
        assert lines[1]["state"]["memory"][0x4a04] == 1
        profile = prototype_profile(profile,root)

        geo = bytearray(1026)
        geo[515] = 0x81  # row 0, column 1
        (root / "GEO.DAX").write_bytes(dax(bytes(geo)))
        (root / "ECL.DAX").write_bytes(dax(bytes(
            [1, 1, 20, 0x99] * 5 + [9, 1, 0x4f, 0xc0, 1, 0, 0x4a, 0x11, 0x80, 1, 4, 0])))
        (root / "ECL_MENU.DAX").write_bytes(dax(bytes(
            [1, 1, 20, 0x99] * 5 + [0x15, 1, 0, 0x4b,
                0x80, 1, 4, 0, 2, 0x80, 1, 4, 0x80, 1, 8, 0])))
        (root / "ECL_UNSUPPORTED.DAX").write_bytes(dax(bytes([1, 1, 20, 0x99] * 5 + [0xff])))

        base = [executable, "--harness", profile, str(root / "GEO.DAX"), "7",
                str(root / "ECL.DAX"), "7", str(root / "state.json")]
        unsupported = [executable, "--harness", profile, str(root / "GEO.DAX"), "7",
                       str(root / "ECL_UNSUPPORTED.DAX"), "7", str(root / "unsupported.json")]

        # Happy path: ready, valid event output, blocked movement, save, quit.
        finished, lines = run_lines(base, actions("east", "north", "save", "quit"))
        identity_checks(lines[0])
        identity = lines[0]["source_identity"]
        check_event(lines[1], 1)
        check_complete(lines[2], 1, "ok", 0, 1, 1)
        assert lines[2]["state"]["memory"][0x4a00] == 0x81
        check_complete(lines[3], 2, "blocked", 0, 1, 0)
        check_complete(lines[4], 3, "ok", 0, 1, 0)
        check_complete(lines[5], 4, "ok", 0, 1, 0)
        assert finished.returncode == 0 and not finished.stderr
        assert finished.stdout.count("\n") == len(lines)

        # CRLF tolerance with in-session save/load round trip.
        fresh_save = base[:-1] + [str(root / "crlf.json")]
        finished, lines = run_lines(fresh_save, actions("east", "save", "load", "quit", crlf=True))
        check_complete(lines[2], 1, "ok", 0, 1, 1)
        check_complete(lines[3], 2, "ok", 0, 1, 1)
        check_complete(lines[4], 3, "ok", 0, 1, 1)
        check_complete(lines[5], 4, "ok", 0, 1, 1)
        assert finished.returncode == 0

        # A fresh process restores the first run's position and memory.
        finished, lines = run_lines(base, actions("load", "quit"))
        check_complete(lines[1], 1, "ok", 0, 1, 0)
        assert lines[1]["state"]["memory"][0x4a00] == 0x81
        assert finished.returncode == 0

        # WHO uses the existing menu protocol with normalized party state.
        (root / "ECL_WHO.DAX").write_bytes(dax(bytes(
            [1, 1, 20, 0x99] * 5 + [0x39, 0x80, 1, 4, 0x11, 0x81, 0, 0x6b, 0])))
        who = Harness(executable, profile, root, ecl="ECL_WHO.DAX", save="who.json")
        identity_checks(who.event())
        who.send({"id": 1, "action": "save"})
        checkpoint = who.event()["state"]
        who.send({"id": 2, "action": "quit"}); who.event()
        assert who.finish()[0] == 0
        checkpoint["party"]["characters"] = [
            {"name": name, "race": "human", "gender": "female", "class": "cleric",
             "alignment": "lawful-good", "abilities": [{"value": 15, "percentile": 0}] * 6}
            for name in ("Ada", "Borin")]
        (root / "who.json").write_text(json.dumps(checkpoint))
        who = Harness(executable, profile, root, ecl="ECL_WHO.DAX", save="who.json")
        who.event(); who.send({"id": 1, "action": "load"}); who.event()
        who.send({"id": 2, "action": "search"})
        prompt = who.event()
        assert prompt["type"] == "menu" and prompt["prompt"] == "A"
        assert prompt["choices"] == ["Ada", "Borin"] and prompt["vertical"] is True
        who.send({"id": 2, "menu_id": prompt["menu_id"], "choice": 1})
        assert who.event()["text"] == "Borin"
        selected = who.event()["state"]
        assert selected["loadedCharacter"] == 1 and selected["memory"][0x6db1] == 1
        who.send({"id": 3, "action": "quit"}); who.event()
        assert who.finish()[0] == 0

        # EOF without quit reports a protocol error and nonzero exit.
        finished, lines = run_lines(base, actions("search"))
        assert lines[-1]["type"] == "error" and finished.returncode != 0

        # Malformed JSON, wrong id types/values and unknown or missing actions.
        for request, needle in [
                ("{oops", ""),  # invalid JSON: only the error shape is asserted
                ({"action": "quit"}, "id field"),  # missing id
                ({"id": 0, "action": "quit"}, "positive"),  # zero id
                ({"id": -1, "action": "quit"}, "nonnegative"),  # negative id
                ({"id": 1.5, "action": "quit"}, "integer"),  # double id
                ({"id": "1", "action": "quit"}, "integer"),  # string id
                ({"id": None, "action": "quit"}, "integer"),  # null id
                ({"id": True, "action": "quit"}, "integer"),
                (None, "JSON object"),
                ({"id": 1}, "action"),  # missing action
                ({"id": 1, "action": 42}, "string"),  # typed action
                ({"id": 1, "action": "stampede"}, "Unknown harness action"),  # unknown
                ({"id": 1, "action": "quit\x00extra"}, "Unknown harness action"),
        ]:
            text = request if isinstance(request, str) else json.dumps(request)
            finished, lines = run_lines(base, text + "\n")
            assert finished.returncode != 0 and len(lines) == 2, (request, lines)
            assert lines[-1]["type"] == "error" and needle in lines[-1]["message"], (request, lines)

        # Non-monotonic request ids are rejected mid-session.
        finished, lines = run_lines(base, '{"id": 2, "action": "north"}\n{"id": 2, "action": "quit"}\n')
        assert lines[1]["type"] == "complete"
        assert lines[2]["type"] == "error" and "strictly increase" in lines[2]["message"]
        assert finished.returncode != 0

        # Each input line is bounded to 64KiB before any further accumulation.
        finished, lines = run_lines(base, '{"id": 1, "action": "' + "n" * (64 * 1024) + '"}\n')
        assert finished.returncode != 0
        assert lines[-1]["type"] == "error" and "64 KiB" in lines[-1]["message"]
        maximum_line = json.dumps({"id": 1, "action": "quit"})
        finished, lines = run_lines(base, maximum_line.ljust(64 * 1024) + "\n")
        assert finished.returncode == 0 and lines[-1]["type"] == "complete"

        finished, lines = run_lines([executable, "--harness"], "")
        assert finished.returncode != 0 and len(lines) == 1 and lines[0]["type"] == "error"

        # Menu events are flushed before the harness reads the response.
        menu = Harness(executable, profile, root, ecl="ECL_MENU.DAX", save="menu-state.json")
        identity_checks(menu.event())
        menu.send({"id": 1, "action": "search"})
        prompt = menu.event()
        assert prompt["type"] == "menu" and prompt["id"] == 1 and prompt["menu_id"] == 1
        assert prompt["prompt"] == "A" and prompt["choices"] == ["A", "B"]
        assert prompt["vertical"] is True
        menu.send('{"id": 1, "menu_id": 1, "choice": 1}')
        document = menu.event()
        check_complete(document, 1, "ok", 0, 0, 0)
        assert document["state"]["memory"][0x4b00] == 1  # chosen selection persisted
        menu.send({"id": 2, "action": "quit"})
        check_complete(menu.event(), 2, "ok", 0, 0, 0)
        assert menu.finish() == (0, "")


        # Wrong response ids, menu ids, types and out-of-range choices fail.
        for response in [
                {"id": 2, "menu_id": 1, "choice": 0},  # stale action id
                {"id": 1, "menu_id": 2, "choice": 0},  # wrong menu id
                {"id": 1, "menu_id": 1},               # missing choice
                {"id": 1, "menu_id": 1, "choice": 2},  # out of range
                {"id": 1, "menu_id": 1, "choice": -1},  # negative choice
                {"id": 1, "menu_id": 1, "choice": 0.5},  # double choice
                {"id": 1, "menu_id": 1, "choice": True},
                None,
                "{bad-json",
        ]:
            run = Harness(executable, profile, root, ecl="ECL_MENU.DAX", save="menu-state.json")
            identity_checks(run.event())
            run.send({"id": 1, "action": "search"})
            menu_prompt = run.event()
            assert menu_prompt["type"] == "menu" and menu_prompt["id"] == 1 and menu_prompt["menu_id"] == 1
            run.send(response if isinstance(response, str) else json.dumps(response))
            error = run.event()
            assert error["type"] == "error", (response, error)
            assert run.finish()[0] != 0

        # EOF and cancellation also release a child waiting for a menu reply.
        waiting = Harness(executable, profile, root, ecl="ECL_MENU.DAX")
        identity_checks(waiting.event())
        waiting.send({"id": 1, "action": "search"})
        assert waiting.event()["type"] == "menu"
        assert waiting.finish()[0] != 0
        assert waiting.event()["type"] == "error"
        cancelled = Harness(executable, profile, root, ecl="ECL_MENU.DAX")
        identity_checks(cancelled.event())
        cancelled.send({"id": 1, "action": "search"})
        assert cancelled.event()["type"] == "menu"
        cancelled.close()
        assert cancelled.process.poll() is not None

        # The VM failure state is surfaced as "unsupported", never "ok".
        finished, lines = run_lines(unsupported, actions("east", "quit"))
        check_complete(lines[1], 1, "unsupported", 0, 1, 1)
        assert "Unknown ECL opcode" in lines[1]["diagnostic"]
        check_complete(lines[2], 2, "ok", 0, 1, 1)
        assert finished.returncode == 0

        # Saves refuse overwriting and reject source changes on load.
        finished, lines = run_lines(base, actions("save", "save"))
        assert lines[-1]["type"] == "error" and "Refusing to overwrite" in lines[-1]["message"]
        assert finished.returncode != 0
        geo[515] = 0x82  # changed content must invalidate prior save identities
        (root / "GEO.DAX").write_bytes(dax(bytes(geo)))
        finished, lines = run_lines(base, actions("load"))
        assert lines[0]["source_identity"] != identity
        assert lines[-1]["type"] == "error" and "mismatch" in lines[-1]["message"]
        assert finished.returncode != 0

    print("harness protocol tests passed")


if __name__ == "__main__":
    try:
        main()
    finally:
        for harness in list(Harness.active):
            harness.close()
