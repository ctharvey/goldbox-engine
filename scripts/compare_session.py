# SPDX-License-Identifier: GPL-3.0-or-later
"""Protocol rendering helpers and the operator Session loop for compare_game."""
import json
import queue
import subprocess
import threading
import time
from compare_common import COMMANDS, ControlledError, ENGINE_QUIT_DRAIN_SECONDS, EVENT_WAIT_SLICE
from compare_config import _reject
from compare_stream import _wait_bounded
from compare_wsl import WslEngine
from reference_input import ReferenceInputError, WindowsMessages, key_sequence


RENDER_FIELD_LIMIT = 4096


def _clean(text):
    """Engine text is untrusted: drop control characters (terminal escapes) and
    bound each printed field."""
    kept = "".join(ch if ch in "\n\t" or (ch.isprintable() and ch != "\x7f") else "?"
                   for ch in text[:RENDER_FIELD_LIMIT])
    return kept + (" [truncated]" if len(text) > RENDER_FIELD_LIMIT else "")


def _count(document, name, what, maximum=None):
    value = document.get(name)
    if isinstance(value, bool) or not isinstance(value, int) or value < 0             or (maximum is not None and value > maximum):
        _reject(value, f"{what} {name}")
    return value


def _render_ready(document, dual=False):
    if document.get("type") != "ready":
        raise ControlledError(
            f"engine protocol violation: first event type {document.get('type')!r}, not ready")
    protocol = document.get("protocol")
    if isinstance(protocol, bool) or not isinstance(protocol, int) or protocol != 1:
        _reject(protocol, "engine protocol version")
    if document.get("baseline") != "unverified-empty":
        _reject(document.get("baseline"), "engine baseline")
    identity = document.get("source_identity")
    if not isinstance(identity, str) or not identity:
        _reject(identity, "engine ready source_identity")
    parts = []
    for field in ("row", "column", "facing"):
        value = _count(document, field, "engine ready", 3 if field == "facing" else None)
        parts.append(f"{field} {value}")
    print("[engine ready]")
    print(f"  protocol 1; source_identity={_clean(identity[:64])}")
    print("  " + ", ".join(parts) + "; baseline unverified-empty")
    print("[note] this comparison is unpaired and inconclusive; no parity result is produced.")
    if not dual:
        print("[note] the reference window is driven manually; this console only drives the")
        print("[note] rebuilt engine side. Do not paste proprietary original text here.")


def _render_text(document):
    text = document.get("text")
    if not isinstance(text, str):
        _reject(text, "engine text value")
    if not isinstance(document.get("clear"), bool):
        _reject(document.get("clear"), "engine text clear flag")
    if text:
        print(_clean(text))


def _validate_menu(document):
    choices = document.get("choices")
    if not isinstance(choices, list) or not choices             or any(not isinstance(item, str) for item in choices):
        _reject(choices, "engine menu choices")
    prompt = document.get("prompt")
    if not isinstance(prompt, str):
        _reject(prompt, "engine menu prompt")
    if not isinstance(document.get("vertical"), bool):
        _reject(document.get("vertical"), "engine menu vertical flag")
    return choices, prompt


def _print_menu(choices, prompt):
    if prompt:
        print(_clean(prompt))
    print("menus accept a zero-based choice number shown alongside each entry.")
    for index, choice in enumerate(choices):
        print(f"  [{index}] {_clean(choice)}")


def _offer_menu(document):
    choices, prompt = _validate_menu(document)
    _print_menu(choices, prompt)
    return choices


def _validate_complete(document):
    status = document.get("status")
    if not isinstance(status, str) or status not in ("ok", "blocked", "unsupported"):
        _reject(status, "engine complete status")
    state = document.get("state")
    if not isinstance(state, dict):
        _reject(state, "engine complete state")
    for name in ("row", "column", "facing"):
        _count(document, name, "engine complete", 3 if name == "facing" else None)
    dispatches = document.get("dispatches")
    if not isinstance(dispatches, list):
        _reject(dispatches, "engine complete dispatches")
    for dispatch in dispatches:
        if not isinstance(dispatch, dict):
            _reject(dispatch, "engine dispatch entry")
        for name in ("table_offset", "selector", "target_offset"):
            _count(dispatch, name, "engine dispatch")
        if not isinstance(dispatch.get("subroutine"), bool):
            _reject(dispatch.get("subroutine"), "engine dispatch subroutine flag")
    diagnostic = document.get("diagnostic")
    if not isinstance(diagnostic, str):
        _reject(diagnostic, "engine complete diagnostic")
    for name in ("steps", "bytecode_offset"):
        _count(document, name, "engine complete")
    return status, state, dispatches, diagnostic


def _render_complete(document):
    status, state, dispatches, diagnostic = _validate_complete(document)
    position = " ".join(f"{name}={document.get(name)!r}" for name in ("row", "column", "facing"))
    print(f"[complete id={document.get('id')!r}] status={status}; {position}")
    for dispatch in dispatches:
        print("  dispatch: " + ", ".join(
            f"{name}={dispatch.get(name)!r}" for name in
            ("table_offset", "selector", "target_offset", "subroutine")))
    if status == "unsupported":
        print(f"  unsupported diagnostic: {_clean(diagnostic)}")
    if status == "blocked":
        print("  move was blocked; no event executed")
    print(f"  steps={document.get('steps')!r} "
          f"bytecode_offset={document.get('bytecode_offset')!r}; "
          f"state captured ({len(json.dumps(state))} bytes, not rendered)")


class Session:
    """Strict single-flight operator loop over the engine protocol."""

    def __init__(self, config, engine, reference, engine_queue, console, deadline):
        self.config = config
        self.engine = engine
        self.reference = reference
        self.engine_queue = engine_queue
        self.console = console
        self.deadline = deadline
        self.next_id = 1
        self.pending_id = None
        self.pending_menu = None
        self.quit_requested = False
        self.last_summary = None
        self.last_checkpoint = time.monotonic()
        self.last_event_seen = time.monotonic()
        self.last_menu_id = 0
        self.end_notes = []
        self.drain_watch = []
        self.reference_input = None
        self.armed = False
        self.facing = None
        if config["reference"].get("input") is not None:
            self.reference_input = WindowsMessages(reference.process, config["reference"]["input"])

    def run(self):
        self._await_ready()
        while True:
            self._check_housekeeping()
            if self.quit_requested and self.pending_id is None:
                return self._finish_streamdown()
            self._heartbeat()
            if self.pending_menu is not None:
                self._serve_menu_choice()
                continue
            if self.pending_id is not None:
                self._serve_action()
                continue
            self._serve_operator()

    def _check_housekeeping(self):
        if isinstance(self.engine, WslEngine):
            self.engine.heartbeat()
        if self.engine_queue.failure is not None:
            raise self.engine_queue.failure
        for drain in self.drain_watch:
            if drain is not None and drain.failure is not None:
                raise drain.failure
        self._check_reference()

    def _finish_streamdown(self):
        remainder = max(self.deadline - time.monotonic(), 0.0)
        budget = min(max(remainder, 0.5), ENGINE_QUIT_DRAIN_SECONDS)
        drain_deadline = time.monotonic() + budget  # one budget for exit and EOF together
        while True:
            # Sliced so the WSL lease keeps being renewed while the engine exits.
            try:
                self.engine.process.wait(
                    timeout=max(min(EVENT_WAIT_SLICE, drain_deadline - time.monotonic()), 0.0))
                break
            except subprocess.TimeoutExpired as error:
                if time.monotonic() >= drain_deadline:
                    raise ControlledError(
                        "engine did not exit after acknowledging quit") from error
                self._check_housekeeping()
        code = self.engine.process.returncode
        if code != 0:
            raise ControlledError(
                f"engine exited with code {code} after acknowledging quit")
        while True:
            self._check_housekeeping()
            leftover = drain_deadline - time.monotonic()
            if leftover <= 0:
                raise ControlledError(
                    "engine stdout did not reach EOF within the post-ack drain budget")
            try:
                token = self.engine_queue.queue.get(timeout=min(leftover, EVENT_WAIT_SLICE))
            except queue.Empty:
                continue
            if token[0] != "eof":
                raise ControlledError(
                    f"engine emitted an extra {token[1].get('type')!r} event "
                    "after the quit acknowledgment")
            break
        _wait_bounded(self.engine_queue.reader)
        if self.engine_queue.failure is not None:
            raise self.engine_queue.failure
        self.end_notes = [
            f"[done] streamdown verified: engine exit code {code}, stdout drained; "
            f"{remainder:0.0f}s session budget unused.",
            "compare_game: session ended; no comparison result (unpaired preview "
            "is inconclusive by design)."]
        return 0

    def _check_reference(self):
        code = self.reference.process.poll()
        if code is not None:
            raise ControlledError(
                f"reference exited with code {code} while the session was active; "
                "session inconclusive")

    def _await_ready(self):
        document = self._next_event("engine never became ready")
        _render_ready(document, self.reference_input is not None)
        self._observe(document)
        self.facing = document["facing"]
        if self.reference_input is not None:
            print("[dual input] disarmed. Use 'reference KEY ...' to stage the original, then 'arm'.")
        self.last_summary = "ready"

    def _heartbeat(self):
        remaining = self.deadline - time.monotonic()
        if remaining <= 0:
            raise ControlledError(f"{self.config['timeout']:0.0f}s session deadline exceeded")
        if time.monotonic() - self.last_checkpoint >= self.config["checkpoint"]:
            self.last_checkpoint = time.monotonic()
            print(f"[checkpoint {self.config['timeout'] - remaining:0.0f}s elapsed; "
                  f"{remaining:0.0f}s remain; last engine event: {self.last_summary}]")
            if self.reference_input is None:
                print("[note] original-side manual tiling: issue the corresponding action in the "
                      "reference window's own console; this controller never sends it keys.")
            else:
                print("[dual input] one action stream; reference state remains independently unverified")

    def _next_event(self, timeout_note):
        while True:
            self._check_housekeeping()
            remaining = self.deadline - time.monotonic()
            if remaining <= 0:
                raise ControlledError(f"{self.config['timeout']:0.0f}s session deadline "
                                      f"({timeout_note})")
            try:
                token = self.engine_queue.queue.get(timeout=min(remaining, EVENT_WAIT_SLICE))
            except queue.Empty:
                self._heartbeat()
                if time.monotonic() - self.last_event_seen >= self.config["checkpoint"]:
                    raise ControlledError(
                        "engine produced no event within the checkpoint window ("
                        f"{self.config['checkpoint']:0.0f}s); session inconclusive")
                continue
            self._heartbeat()
            self.last_event_seen = time.monotonic()
            if token[0] == "eof":
                if self.engine_queue.failure is not None:
                    raise self.engine_queue.failure
                code = self.engine.process.poll()
                raise ControlledError(
                    f"engine closed stdout before quit (exit code {code}); session inconclusive")
            if token[0] == "event":
                return token[1]

    def _observe(self, document):
        observer = getattr(self.console, 'observe', None)
        if observer is not None:
            observer(document)

    def _ignored(self, message):
        if getattr(self.console, 'observe', None) is None:
            print(f'[ignored] {message}')
        else:
            raise ControlledError(f'scenario command rejected: {message}')

    def _serve_operator(self):
        if self.console.failure is not None:
            raise self.console.failure
        self._pump_idle_events()
        remaining = self.deadline - time.monotonic()
        if remaining <= 0:
            raise ControlledError(f"{self.config['timeout']:0.0f}s session deadline exceeded")
        try:
            item = self.console.queue.get(timeout=min(remaining, EVENT_WAIT_SLICE))
        except queue.Empty:
            return
        if item is None:
            raise ControlledError("operator console reached EOF before quit; "
                                  "session ends inconclusively")
        if not item:
            return
        verb = item.lower()
        if verb in ("arm", "disarm") or verb.startswith("reference "):
            if self.reference_input is None:
                raise ControlledError("reference input is not configured")
            if verb == "arm":
                try:
                    self.reference_input.window()
                except ReferenceInputError as error:
                    self._ignored(f"cannot arm yet: {error}")
                    return
                self.armed = True
                print("[dual input] armed; action/menu keys go to both sides. Baseline remains unverified.")
            elif verb == "disarm":
                self.armed = False
                print("[dual input] disarmed")
            else:
                if self.armed:
                    raise ControlledError("disarm before sending reference-only staging keys")
                try:
                    keys = key_sequence(item.upper().split()[1:])
                except ReferenceInputError as error:
                    self._ignored(f"nothing was sent: {error}")
                    return
                self._send(None, keys)
                print("[reference] staging keys delivered")
            return
        if verb == "help":
            print("accepted engine commands: " + ", ".join(COMMANDS))
            if self.reference_input is not None:
                print("dual input: reference KEY ..., arm, disarm; quit closes both owned trees")
            print("[note] menus accept a zero-based choice number shown alongside each entry.")
            return
        if verb not in COMMANDS:
            self._ignored(f"unknown command {item!r}; type 'help' for the accepted list")
            return
        self._dispatch(verb)

    def _pump_idle_events(self):
        while not self.engine_queue.queue.empty():
            token = self.engine_queue.queue.get_nowait()
            if token[0] == "event":
                if token[1].get("type") == "error":
                    self._consume(token[1])
                raise ControlledError(
                    f"out-of-order engine event {token[1].get('type')!r} outside an action")
            if self.engine_queue.failure is not None:
                raise self.engine_queue.failure
            code = self.engine.process.poll()
            raise ControlledError(
                f"engine exited before quit (exit code {code}); session inconclusive")
        if self.engine_queue.failure is not None:
            raise self.engine_queue.failure

    def _dispatch(self, command):
        """Issue the semantic request with the next strictly increasing id."""
        self._pump_idle_events()
        keys = None
        if self.reference_input is not None and command != "quit":
            if not self.armed:
                raise ControlledError("dual input is disarmed; stage both sides before 'arm'")
            keys = self.reference_input.mapping.action(command, self.facing)
        action_id = self.next_id
        self.next_id += 1
        self._send({"id": action_id, "action": command}, keys)
        self.pending_id = action_id
        self.last_event_seen = time.monotonic()
        if command == "quit":
            self.quit_requested = True
        elif self.reference_input is None:
            print("[note] manual original-side step: issue the same action in the "
                  "reference window; this console only drives the rebuilt engine side.")
        print(f"[engine] sent action id {action_id}: {command}")
        if keys is not None:
            print(f"[dual input id={action_id}] delivered to both sides (baseline unverified)")

    def _send(self, document, reference_keys=None):
        failures = []
        gate = threading.Event()
        limit = min(self.deadline, time.monotonic() + self.config["checkpoint"])

        def write():
            self.engine.process.stdin.write((json.dumps(document) + "\n").encode("utf-8"))
            self.engine.process.stdin.flush()

        def worker(action):
            gate.wait()
            try:
                action()
            except Exception as error:  # A failed worker must never look like delivered input.
                failures.append(error)

        cancel = threading.Event()
        actions = [write] if document is not None else []
        if reference_keys is not None:
            self.reference_input.cancel = cancel
            actions.append(lambda: self.reference_input.send(reference_keys, limit))
        threads = [threading.Thread(target=worker, args=(action,), daemon=True) for action in actions]
        for thread in threads:
            thread.start()
        gate.set()
        try:
            while any(thread.is_alive() for thread in threads):
                self._check_housekeeping()
                if time.monotonic() >= limit:
                    raise ControlledError("input dispatch exceeded its bounded deadline; delivery may be partial")
                time.sleep(0.01)
        except BaseException:
            cancel.set()  # No further reference key may start after we gave up.
            raise
        if failures:
            raise ControlledError(f"input dispatch failed; delivery may be partial, do not retry: {failures[0]}")

    def _serve_action(self):
        document = self._next_event(f"action {self.pending_id} never completed")
        self._consume(document)

    def _serve_menu_choice(self):
        remaining = self.deadline - time.monotonic()
        if remaining <= 0:
            raise ControlledError(f"{self.config['timeout']:0.0f}s session deadline exceeded "
                                  "while a menu awaited input")
        action_id, menu_id, choices = self.pending_menu
        if self.console.failure is not None:
            raise self.console.failure
        try:
            item = self.console.queue.get(timeout=min(remaining, EVENT_WAIT_SLICE))
        except queue.Empty:
            return
        if item is None:
            raise ControlledError("operator console reached EOF while a menu awaited input")
        stripped = item.strip()
        if not stripped:
            return
        try:
            choice = int(stripped)
        except ValueError:
            self._ignored("menu choice requires a decimal number; re-enter it")
            return
        if not 0 <= choice < len(choices):
            self._ignored(f"menu choice {choice} lies outside the offered range "
                          f"[0, {len(choices) - 1}]; re-enter it")
            return
        keys = None
        if self.reference_input is not None:
            keys = self.reference_input.mapping.choice(menu_id, choice, len(choices))
        self.pending_menu = None
        self.last_event_seen = time.monotonic()
        self._send({"id": action_id, "menu_id": menu_id, "choice": choice}, keys)
        print(f"[engine] sent menu response: action id {action_id}, menu id {menu_id}, "
              f"choice {choice}")
        if keys is not None:
            print(f"[dual input id={action_id} menu={menu_id}] delivered to both sides")

    def _consume(self, document):
        event_type = document.get("type")
        if event_type == "error":
            message = document.get("message")
            if not isinstance(message, str) or not message:
                _reject(document.get("message"), "engine error event message")
            raise ControlledError(f"engine emitted an error event: {message}")
        if self.pending_id is None:
            raise ControlledError(
                f"out-of-order engine event type {event_type!r} outside an action")
        event_id = document.get("id")
        if isinstance(event_id, bool) or not isinstance(event_id, int):
            raise ControlledError(f"engine event id is not an integer: {event_id!r}")
        if event_id != self.pending_id:
            raise ControlledError(
                f"out-of-order engine event id {event_id}: pending action id {self.pending_id}")
        if event_type == "text":
            _render_text(document)
            self._observe(document)
            self.last_summary = f"text (action {event_id})"
            return
        if event_type == "menu":
            if self.pending_menu is not None:
                raise ControlledError(
                    "engine sent a second menu before the pending menu was answered")
            choices, prompt = _validate_menu(document)
            menu_id = document.get("menu_id")
            if isinstance(menu_id, bool) or not isinstance(menu_id, int) or menu_id < 1:
                _reject(menu_id, "engine menu_id")
            if menu_id <= self.last_menu_id:
                raise ControlledError(
                    f"engine menu_id {menu_id} is not strictly greater than the "
                    f"previous menu id {self.last_menu_id}")
            _print_menu(choices, prompt)
            self.last_menu_id = menu_id
            self.pending_menu = (self.pending_id, menu_id, choices)
            self._observe(document)
            self.last_summary = f"menu event (action {event_id}, menu {menu_id})"
            return
        if event_type == "complete":
            _render_complete(document)
            self.facing = document["facing"]
            if self.reference_input is not None and document["status"] == "unsupported":
                raise ControlledError("engine action unsupported; dual input stopped before another action")
            if self.quit_requested and document["status"] != "ok":
                raise ControlledError("engine did not successfully acknowledge quit")
            self._observe(document)
            self.last_summary = f"complete status {document.get('status')!r} (id {event_id})"
            self.pending_id = None
            return
        raise ControlledError(f"unknown engine event type {event_type!r}")
