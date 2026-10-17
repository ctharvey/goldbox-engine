# SPDX-License-Identifier: GPL-3.0-or-later
"""Shared constants and error types for the compare_game controller."""

SCHEMA_VERSION = 1
DEFAULT_TIMEOUT = 60
MIN_TIMEOUT = 1
MAX_TIMEOUT = 600
DEFAULT_CHECKPOINT = 10
ENGINE_LINE_LIMIT = 8 * 1024 * 1024
ENGINE_OUTPUT_LIMIT = 64 * 1024 * 1024
REFERENCE_OUTPUT_LIMIT = 64 * 1024 * 1024
EVENT_QUEUE_LIMIT = 256
OPERATOR_LINE_LIMIT = 8192
OPERATOR_QUEUE_LIMIT = 64
ENGINE_QUIT_DRAIN_SECONDS = 5.0
EVENT_WAIT_SLICE = 0.5
CHUNK = 65536
COMMANDS = ("north", "east", "south", "west", "search", "rest",
            "init", "save", "load", "quit", "help")


class ControlledError(Exception):
    """Operator-relevant controller failure; always exits through cleanup."""


class FloodError(ControlledError):
    """Captured output or queued events exceeded a bounded limit."""
