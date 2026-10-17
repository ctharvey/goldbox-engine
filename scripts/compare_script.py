# SPDX-License-Identifier: GPL-3.0-or-later
"""Bounded deterministic input and event assertions for the existing controller."""
import json
from pathlib import Path
import queue
import re

from compare_common import COMMANDS, ControlledError, OPERATOR_LINE_LIMIT, OPERATOR_QUEUE_LIMIT
from reference_input import key_sequence, ReferenceInputError


def pointer(document, path):
    if path == '':
        return document
    if not isinstance(path, str) or not path.startswith('/') or re.search(r'~(?![01])', path):
        raise ControlledError('scenario check requires a valid JSON pointer')
    value = document
    for token in path.split('/')[1:]:
        token = token.replace('~1', '/').replace('~0', '~')
        if isinstance(value, dict) and token in value:
            value = value[token]
        elif isinstance(value, list) and re.fullmatch(r'0|[1-9][0-9]*', token) and int(token) < len(value):
            value = value[int(token)]
        else:
            raise ControlledError(f'scenario event is missing check path {path!r}')
    return value


def equal(actual, expected):
    # JSON booleans must not compare equal to integers, even inside containers.
    if type(actual) is not type(expected):
        return False
    if isinstance(expected, dict):
        return actual.keys() == expected.keys() and all(equal(actual[key], value) for key, value in expected.items())
    if isinstance(expected, list):
        return len(actual) == len(expected) and all(equal(a, b) for a, b in zip(actual, expected))
    return actual == expected


class ScriptInput:
    """Console-compatible scripted source. No stdin thread, LLM, sleeps or retries.

    The existing Session controls pacing and dual-input mapping. An observer
    checks every validated engine event before the session can proceed.
    """
    def __init__(self, document):
        if (not isinstance(document, dict) or set(document) != {'schemaVersion', 'commands', 'expect'}
                or type(document['schemaVersion']) is not int or document['schemaVersion'] != 1):
            raise ControlledError('scenario requires schemaVersion 1, commands and expect only')
        self.commands = document['commands']
        self.expected = document['expect']
        if not isinstance(self.commands, list) or not 1 <= len(self.commands) < OPERATOR_QUEUE_LIMIT:
            raise ControlledError('scenario requires 1..63 commands')
        for command in self.commands:
            if (not isinstance(command, str) or not command or command != command.strip()
                    or len(command.encode('utf-8')) > OPERATOR_LINE_LIMIT or '\n' in command or '\r' in command):
                raise ControlledError('invalid or oversized scenario command')
            verb = command.lower()
            if verb.startswith('reference '):
                try:
                    key_sequence(command.upper().split()[1:])
                except ReferenceInputError as error:
                    raise ControlledError(f'invalid reference staging command: {error}') from error
            elif verb not in (*COMMANDS, 'arm', 'disarm') and not re.fullmatch(r'[0-9]+', command):
                raise ControlledError(f'unknown scenario command {command!r}')
        if self.commands[-1].lower() != 'quit' or any(c.lower() == 'quit' for c in self.commands[:-1]):
            raise ControlledError('scenario must end with exactly one quit command')
        if not isinstance(self.expected, list) or not 2 <= len(self.expected) <= 256:
            raise ControlledError('scenario requires 2..256 expected events')
        for event in self.expected:
            if (not isinstance(event, dict) or set(event) != {'type', 'checks'}
                    or event['type'] not in ('ready', 'text', 'menu', 'complete')
                    or not isinstance(event['checks'], dict)):
                raise ControlledError('scenario events require type and checks')
            for path, value in event['checks'].items():
                if not isinstance(path, str) or (path and not path.startswith('/')) or re.search(r'~(?![01])', path):
                    raise ControlledError('scenario check requires a valid JSON pointer')
                try:
                    json.dumps(value, allow_nan=False)
                except (ValueError, TypeError) as error:
                    raise ControlledError('scenario checks require finite JSON values') from error
            if event['type'] != 'ready' and type(event['checks'].get('/id')) is not int:
                raise ControlledError('each action event must check its integer /id')
            if event['type'] == 'complete' and '/status' not in event['checks']:
                raise ControlledError('each completion must check /status')
        if self.expected[0]['type'] != 'ready' or self.expected[-1]['type'] != 'complete':
            raise ControlledError('scenario must check ready first and quit completion last')
        if self.expected[-1]['checks']['/status'] != 'ok':
            raise ControlledError('scenario must expect a successful quit acknowledgment')
        self.queue = queue.Queue(maxsize=OPERATOR_QUEUE_LIMIT)
        self.failure = None
        self.seen = 0

    @classmethod
    def from_file(cls, path):
        try:
            with Path(path).open('rb') as source:
                data = source.read(256 * 1024 + 1)
            if len(data) > 256 * 1024:
                raise ControlledError('scenario exceeds 256 KiB')
            return cls(json.loads(data))
        except (OSError, ValueError, RecursionError) as error:
            raise ControlledError(f'cannot read scenario: {error}') from error

    def start(self):
        for command in self.commands:
            self.queue.put_nowait(command)
        self.queue.put_nowait(None)

    def observe(self, document):
        if self.seen >= len(self.expected):
            raise ControlledError('scenario received an unexpected extra engine event')
        expected = self.expected[self.seen]
        number = self.seen + 1
        if document.get('type') != expected['type']:
            raise ControlledError(f'scenario event {number}: expected {expected["type"]}, got {document.get("type")}')
        for path, value in expected['checks'].items():
            if not equal(pointer(document, path), value):
                raise ControlledError(f'scenario event {number}: check {path!r} mismatched')
        self.seen += 1

    def finish(self):
        if self.seen != len(self.expected):
            raise ControlledError(f'scenario checked {self.seen}/{len(self.expected)} expected events')
        while not self.queue.empty():
            if self.queue.get_nowait() is not None:
                raise ControlledError('scenario ended with unused commands')
        return f'[scripted PASS] {self.seen} engine events checked; reference parity remains unverified.'
