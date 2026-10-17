#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Scripted controller against the real engine and synthetic, bounded resources."""
import contextlib
import copy
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

from compare_game_test import Fixture
from harness_test import dax

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
from compare_common import ControlledError
from compare_config import parse_config
from compare_game import run_session
import compare_game
from compare_script import ScriptInput, equal, pointer

ENGINE, PROFILE = sys.argv[1:3]
del sys.argv[1:3]
CONTROLLER = Path(__file__).resolve().parents[1] / 'scripts' / 'compare_game.py'


def event(kind, **checks):
    return dict(type=kind, checks={'/' + key: value for key, value in checks.items()})


class ScriptedHarnessTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.fixture = Fixture(self.root)
        geo = bytearray(1026)
        geo[515] = 0x81
        (self.root / 'GEO.DAX').write_bytes(dax(bytes(geo)))
        (self.root / 'ECL.DAX').write_bytes(dax(bytes(
            [1, 1, 20, 0x99] * 5 + [9, 1, 0x4f, 0xc0, 1, 0, 0x4a, 0x11, 0x80, 1, 4, 0])))
        self.config = self.fixture.config('rich', timeout=5)
        settings = json.loads(self.config.read_text())
        settings['engine']['command'] = [ENGINE, '--harness', PROFILE, str(self.root / 'GEO.DAX'),
                                         '7', str(self.root / 'ECL.DAX'), '7', str(self.root / 'state.json')]
        self.config.write_text(json.dumps(settings))
        self.scenario = dict(schemaVersion=1, commands=['east', 'save', 'west', 'load', 'quit'], expect=[
            event('ready', protocol=1, row=0, column=0),
            event('text', id=1, text='A'),
            event('complete', id=1, status='ok', column=1, **{'state/memory/18944': 129}),
            event('complete', id=2, status='ok', column=1),
            event('text', id=3, text='A'),  # Both cells dispatch the authored synthetic entry.
            event('complete', id=3, status='ok', column=0, facing=3),
            event('complete', id=4, status='ok', column=1, facing=1, **{'state/memory/18944': 129}),
            event('complete', id=5, status='ok', column=1)])

    def invoke(self, scenario=None):
        path = self.root / 'scenario.json'
        path.write_text(json.dumps(self.scenario if scenario is None else scenario))
        return subprocess.run([sys.executable, str(CONTROLLER), str(self.config), '--script', str(path)],
                              stdin=subprocess.DEVNULL, capture_output=True, text=True, timeout=12)

    def test_repeated_real_engine_route_and_save_restore(self):
        first = self.invoke()
        self.assertEqual(first.returncode, 0, first.stderr)
        saved = json.loads((self.root / 'state.json').read_text())
        (self.root / 'state.json').unlink()  # Restore the same empty synthetic save baseline.
        second = self.invoke()
        self.assertEqual(second.returncode, 0, second.stderr)
        self.assertIn('[scripted PASS] 8 engine events checked', second.stdout)
        self.assertEqual(json.loads((self.root / 'state.json').read_text()), saved)

    def test_programmatic_driver_uses_same_session(self):
        children = []
        launch = compare_game.OwnedProcess.launch
        def tracked_launch(*args, **kwargs):
            child = launch(*args, **kwargs)
            children.append(child)
            return child
        with patch.object(compare_game.OwnedProcess, 'launch', side_effect=tracked_launch), \
                contextlib.redirect_stdout(io.StringIO()):
            code, notes = run_session(parse_config(self.config), ScriptInput(self.scenario))
        self.assertEqual(code, 0)
        self.assertTrue(any('[scripted PASS]' in note for note in notes))
        self.assertEqual(len(children), 2)
        for child in children:
            self.assertTrue(child.process.stdout.closed)
            self.assertTrue(child.process.stderr.closed)

    def test_native_fixture_scripted_input(self):
        # Also runnable on Windows without a native C++ build; the rest of this
        # suite exercises the real engine in CI.
        self.config = self.fixture.config('rich')
        scenario = dict(schemaVersion=1, commands=['search', 'search', '1', 'quit'], expect=[
            event('ready', protocol=1),
            event('text', id=1, text='synthetic search stub text'),
            event('complete', id=1, status='ok'),
            event('text', id=2, text='synthetic search stub text'),
            event('menu', id=2, menu_id=1, choices=['Alpha', 'Beta']),
            event('text', id=2, text='chosen menu entry 1'),
            event('complete', id=2, status='ok', **{'state/memory/2': 2}),
            event('complete', id=3, status='ok')])
        result = self.invoke(scenario)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('[scripted PASS] 8 engine events checked', result.stdout)

    def menu_scenario(self, answer):
        (self.root / 'ECL.DAX').write_bytes(dax(bytes(
            [1, 1, 20, 0x99] * 5 + [0x15, 1, 0, 0x4b,
                0x80, 1, 4, 0, 2, 0x80, 1, 4, 0x80, 1, 8, 0])))
        return dict(schemaVersion=1, commands=['east', *answer, 'quit'], expect=[
            event('ready', protocol=1),
            event('menu', id=1, menu_id=1, choices=['A', 'B']),
            event('complete', id=1, status='ok', **{'state/memory/19200': 1}),
            event('complete', id=2, status='ok')])

    def test_menu_answer_is_code_driven(self):
        result = self.invoke(self.menu_scenario(['1']))
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('[scripted PASS]', result.stdout)

    def test_invalid_menu_answer_cannot_be_silently_retried(self):
        result = self.invoke(self.menu_scenario(['99', '1']))
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('scenario command rejected', result.stderr)

    def test_mismatch_stops_before_next_action_and_releases_session(self):
        wrong = copy.deepcopy(self.scenario)
        wrong['expect'][2]['checks']['/column'] = 9
        result = self.invoke(wrong)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('scenario event 3', result.stderr)
        self.assertNotIn('[scripted PASS]', result.stdout)
        self.assertFalse((self.root / 'state.json').exists())
        self.assertEqual(self.invoke().returncode, 0)  # Cleanup released the same private-root lock.

    def test_extra_and_missing_events_fail(self):
        missing = copy.deepcopy(self.scenario)
        missing['expect'].pop(1)  # Actual text becomes unexpected.
        result = self.invoke(missing)
        self.assertNotEqual(result.returncode, 0)
        extra = copy.deepcopy(self.scenario)
        extra['expect'].append(event('complete', id=6, status='ok'))
        result = self.invoke(extra)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('8/9 expected events', result.stderr)

    def test_silent_engine_hits_deadline_and_cleans_up(self):
        settings = json.loads(self.config.read_text())
        settings['engine']['command'] = [sys.executable, '-c', 'import time; time.sleep(30)']
        settings['timeoutSeconds'] = settings['checkpointSeconds'] = 1
        self.config.write_text(json.dumps(settings))
        result = self.invoke()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('deadline', result.stderr)
        self.assertNotIn('[scripted PASS]', result.stdout)

    def test_invalid_scenarios_are_rejected_before_launch(self):
        for change in [dict(schemaVersion=True), dict(commands=['quit', 'east']),
                       dict(commands=['unknown', 'quit']), dict(commands=['east'] * 64 + ['quit'])]:
            with self.subTest(change=change):
                bad = dict(self.scenario, **change)
                result = self.invoke(bad)
                self.assertNotEqual(result.returncode, 0)
                self.assertNotIn('[engine ready]', result.stdout)

    def test_missing_path_and_exact_json_types(self):
        self.assertFalse(equal(True, 1))
        self.assertFalse(equal({'memory': [True]}, {'memory': [1]}))
        self.assertEqual(pointer({'a/b': {'~': [42]}}, '/a~1b/~0/0'), 42)
        with self.assertRaises(ControlledError):
            pointer({'state': []}, '/state/1')
        missing = copy.deepcopy(self.scenario)
        missing['expect'][0]['checks']['/not-present'] = 1
        self.assertNotEqual(self.invoke(missing).returncode, 0)


if __name__ == '__main__':
    unittest.main()
