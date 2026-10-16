# SPDX-License-Identifier: GPL-3.0-or-later
"""Explicit key mappings and bounded input to an owned SDL 1.2 window.

Independently authored using documented Win32 keyboard messages. No SDL source
is copied. Messages target the reference PID only; no global keyboard injection
or foreground-window switching is used.
"""
import ctypes
import os
import time

KEYS = {"ENTER": (0x0d, False), "ESC": (0x1b, False), "SPACE": (0x20, False),
        "TAB": (0x09, False), "UP": (0x26, True), "DOWN": (0x28, True),
        "LEFT": (0x25, True), "RIGHT": (0x27, True), "HOME": (0x24, True),
        "END": (0x23, True), "BACKSPACE": (0x08, False)}
KEYS.update({chr(value): (value, False) for value in (*range(65, 91), *range(48, 58))})
KEYS.update({f"KP_{value}": (0x60 + value, False) for value in range(10)})
DIRECTIONS = ("north", "east", "south", "west")
ACTIONS = (*DIRECTIONS, "search", "rest", "init", "save", "load")


class ReferenceInputError(ValueError):
    pass


def key_sequence(value):
    if not isinstance(value, list) or not 1 <= len(value) <= 16 \
            or any(not isinstance(key, str) or key not in KEYS for key in value):
        raise ReferenceInputError("reference keys require 1..16 named keys (letters, digits, arrows, KP_0..9)")
    return list(value)


def validate_config(value):
    if not isinstance(value, dict) or value.get("backend") != "windows-messages" \
            or set(value) - {"backend", "actions", "menus", "relativeMovement"}:
        raise ReferenceInputError("reference.input requires the windows-messages backend and known fields")
    actions = value.get("actions", {})
    menus = value.get("menus", {})
    relative = value.get("relativeMovement")
    if not isinstance(actions, dict) or set(actions) - set(ACTIONS):
        raise ReferenceInputError("unknown reference action mapping")
    for sequence in actions.values():
        key_sequence(sequence)
    if not isinstance(menus, dict) or len(menus) > 256:
        raise ReferenceInputError("reference menus require a bounded object keyed by engine menu ID")
    for identifier, choices in menus.items():
        if not isinstance(identifier, str) or not identifier.isdecimal() \
                or str(int(identifier)) != identifier or int(identifier) < 1 \
                or not isinstance(choices, list) or not 1 <= len(choices) <= 256:
            raise ReferenceInputError("invalid reference menu mapping")
        for sequence in choices:
            key_sequence(sequence)
    if relative is not None:
        if not isinstance(relative, dict) or set(relative) != {"forward", "left", "right"} \
                or set(actions).intersection(DIRECTIONS):
            raise ReferenceInputError("relativeMovement needs forward/left/right and no absolute direction mappings")
        for sequence in relative.values():
            if len(key_sequence(sequence)) > 4:
                raise ReferenceInputError("relative movement components allow at most four keys")
    return value


class Mapping:
    def __init__(self, config):
        self.config = validate_config(config)

    def action(self, action, facing):
        relative = self.config.get("relativeMovement")
        if action in DIRECTIONS and relative is not None:
            if isinstance(facing, bool) or not isinstance(facing, int) or not 0 <= facing <= 3:
                raise ReferenceInputError("relative movement requires a valid engine facing")
            turns = (DIRECTIONS.index(action) - facing) % 4
            sequence = ([] if turns == 0 else relative["right"] if turns == 1
                        else relative["right"] * 2 if turns == 2 else relative["left"])
            return key_sequence(sequence + relative["forward"])
        sequence = self.config.get("actions", {}).get(action)
        if sequence is None:
            raise ReferenceInputError(f"no reference mapping for {action}; neither side was dispatched")
        return key_sequence(sequence)

    def choice(self, menu_id, choice, count):
        choices = self.config.get("menus", {}).get(str(menu_id))
        if choices is None or len(choices) != count:
            raise ReferenceInputError(f"no exact reference mapping for menu {menu_id}; neither side was dispatched")
        return key_sequence(choices[choice])


class WindowsMessages:
    """Direct launch required: exactly one SDL_app window for the owned PID."""

    def __init__(self, process, config):
        if os.name != "nt":
            raise ReferenceInputError("windows-messages requires Windows")
        from ctypes import wintypes as wt
        self.process, self.mapping = process, Mapping(config)
        self.user32 = ctypes.WinDLL("user32", use_last_error=True)
        self.callback_type = ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)
        self.user32.EnumWindows.argtypes = [self.callback_type, wt.LPARAM]
        self.user32.EnumWindows.restype = wt.BOOL
        self.user32.GetWindowThreadProcessId.argtypes = [wt.HWND, ctypes.POINTER(wt.DWORD)]
        self.user32.GetWindowThreadProcessId.restype = wt.DWORD
        self.user32.GetClassNameW.argtypes = [wt.HWND, wt.LPWSTR, ctypes.c_int]
        self.user32.GetClassNameW.restype = ctypes.c_int
        self.user32.IsWindowVisible.argtypes = [wt.HWND]
        self.user32.IsWindowVisible.restype = wt.BOOL
        self.user32.SendMessageTimeoutW.argtypes = [wt.HWND, wt.UINT, wt.WPARAM, wt.LPARAM,
                                                  wt.UINT, wt.UINT, ctypes.POINTER(ctypes.c_size_t)]
        self.user32.SendMessageTimeoutW.restype = ctypes.c_ssize_t
        self.user32.MapVirtualKeyW.argtypes = [wt.UINT, wt.UINT]
        self.user32.MapVirtualKeyW.restype = wt.UINT

    def window(self, deadline=None):
        from ctypes import wintypes as wt
        if self.process.poll() is not None:
            raise ReferenceInputError("reference exited before input delivery")
        candidates = []

        def visit(handle, _):
            pid = wt.DWORD()
            self.user32.GetWindowThreadProcessId(handle, ctypes.byref(pid))
            name = ctypes.create_unicode_buffer(128)
            self.user32.GetClassNameW(handle, name, len(name))
            if pid.value == self.process.pid and name.value == "SDL_app" \
                    and self.user32.IsWindowVisible(handle):
                candidates.append(handle)
            return True

        callback = self.callback_type(visit)
        while True:
            candidates.clear()
            if not self.user32.EnumWindows(callback, 0):
                raise ReferenceInputError("cannot enumerate the owned reference window")
            if candidates or deadline is None or time.monotonic() >= deadline:
                break
            if self.process.poll() is not None:
                raise ReferenceInputError("reference exited before its window appeared")
            time.sleep(0.02)
        if len(candidates) != 1:
            raise ReferenceInputError(f"expected one owned SDL_app window; found {len(candidates)}")
        return candidates[0]

    def send(self, sequence, deadline):
        for key in key_sequence(sequence):
            if time.monotonic() + 0.25 >= deadline:
                raise ReferenceInputError("reference input exceeded the dispatch deadline")
            handle = self.window(deadline)  # Recheck ownership before every key; never use a title match.
            virtual_key, extended = KEYS[key]
            scan = self.user32.MapVirtualKeyW(virtual_key, 0)
            flags = 1 | ((scan & 0xff) << 16) | (int(extended) << 24)
            result = ctypes.c_size_t()

            def message(kind, parameter):
                # ERRORONEXIT | ABORTIFHUNG; bounded even if the app stops pumping.
                if not self.user32.SendMessageTimeoutW(handle, kind, virtual_key, parameter,
                                                       0x22, 100, ctypes.byref(result)):
                    raise ReferenceInputError("reference input delivery failed or timed out; do not retry this action")

            try:
                message(0x0100, flags)
                time.sleep(0.04)
            finally:
                message(0x0101, flags | (1 << 30) | (1 << 31))
            time.sleep(0.04)
