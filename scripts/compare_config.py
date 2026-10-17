# SPDX-License-Identifier: GPL-3.0-or-later
"""Session configuration validation for the compare_game controller."""
import json
import os
from pathlib import Path, PurePosixPath
from compare_common import (ControlledError, DEFAULT_CHECKPOINT, DEFAULT_TIMEOUT,
                            MAX_TIMEOUT, MIN_TIMEOUT, SCHEMA_VERSION)
from reference_input import ReferenceInputError, validate_config


def _reject(value, what):
    raise ControlledError(f"{what} is {value!r}")


def _number_field(container, key, default, minimum, maximum, context):
    value = container.get(key, default)
    if isinstance(value, bool) or not isinstance(value, int) \
            or not minimum <= value <= maximum:
        _reject(value, f"{context}.{key} (requires an integer in "
                       f"[{minimum}, {maximum}])")
    return value


def _exact_schema_version(value):
    if isinstance(value, bool) or not isinstance(value, int) \
            or value != SCHEMA_VERSION:
        _reject(value, "configuration.schemaVersion (requires the integer 1)")


def _argv_field(fields, key, context):
    argv = fields.get(key)
    if not isinstance(argv, list) or not argv \
            or any(not isinstance(entry, str) or not entry for entry in argv):
        _reject(argv, f"{context}.{key} (requires a nonempty array of strings)")
    return list(argv)


def _table_field(container, key, parent):
    value = container.get(key)
    if not isinstance(value, dict):
        _reject(value, f"{parent}.{key} (requires a JSON object)")
    return value


def _absolute_dir(value, context):
    if isinstance(value, bool) or not isinstance(value, str) or not value:
        _reject(value, f"{context} (requires a nonempty string path)")
    path = Path(value)
    if not path.is_absolute():
        _reject(value, f"{context} (requires an absolute path)")
    if not path.is_dir():
        _reject(value, f"{context} (requires an existing directory)")
    try:
        return path.resolve()
    except OSError as error:
        raise ControlledError(f"{context} cannot be resolved: {error}") from error


def _contains(parent, child):
    """True when child is parent or lies below it, by file identity.

    Identity (samefile) rather than path text, so UNC/\\\\?\\ aliases, junctions
    and case variants of the same directory cannot slip past a prefix compare.
    Any stat failure counts as "contained" so callers fail closed.
    """
    try:
        current = Path(child).resolve()
        while True:
            if os.path.samefile(parent, current):
                return True
            if current.parent == current:
                return False
            current = current.parent
    except OSError:
        return True


def _outside_repository(path):
    """True only when path neither lies inside nor contains the repository."""
    repository = Path(__file__).resolve().parent.parent
    return not _contains(repository, path) and not _contains(path, repository)


def parse_config(path):
    """Validate the version-1 session configuration without side effects.

    Rejects malformed/null/bool/NaN values, unsupported schema versions,
    empty argv entries and invalid paths before anything is spawned.
    """
    try:
        config = json.loads(path.read_text(encoding="utf-8"))
    except ValueError as error:
        raise ControlledError(f"session configuration is not valid JSON: {error}") from error
    if not isinstance(config, dict):
        raise ControlledError("session configuration must be a JSON object")
    unknown = sorted(set(config) - {"schemaVersion", "privateRoot", "engine",
                                    "reference", "timeoutSeconds", "checkpointSeconds"})
    if unknown:
        raise ControlledError(f"unsupported configuration fields: {unknown}")

    _exact_schema_version(config.get("schemaVersion"))
    private_root = _absolute_dir(config.get("privateRoot"), "configuration.privateRoot")
    if not _outside_repository(private_root):
        raise ControlledError("configuration.privateRoot must be outside the repository")

    engine = _table_field(config, "engine", "configuration")
    reference = _table_field(config, "reference", "configuration")
    backend = engine.get("backend", "native")
    if backend not in ("native", "wsl"):
        _reject(backend, "engine.backend (requires native or wsl)")
    extra = set(engine) - ({"backend", "command", "cwd", "distribution"}
                           if backend == "wsl" else {"backend", "command", "cwd"})
    if extra:
        raise ControlledError(f"unsupported engine fields: {sorted(extra)}")
    extra = set(reference) - {"command", "cwd", "input"}
    if extra:
        raise ControlledError(f"unsupported reference fields: {sorted(extra)}")
    if backend == "wsl":
        if os.name != "nt":
            raise ControlledError("engine.backend wsl requires a Windows controller")
        engine_cwd = engine.get("cwd")
        if not isinstance(engine_cwd, str) or not PurePosixPath(engine_cwd).is_absolute() \
                or "\0" in engine_cwd:
            raise ControlledError("WSL engine.cwd requires an absolute Linux path")
        distribution = engine.get("distribution")
        if not isinstance(distribution, str) or not distribution.strip() \
                or distribution.startswith("-") or "\0" in distribution:
            raise ControlledError("WSL engine.distribution requires a distribution name")
    else:
        engine_cwd = _absolute_dir(engine.get("cwd"), "engine.cwd")
        distribution = None
    reference_cwd = _absolute_dir(reference.get("cwd"), "reference.cwd")
    if not _contains(private_root, reference_cwd):
        raise ControlledError("reference.cwd must be inside privateRoot")
    reference_input = reference.get("input")
    if "input" in reference:
        try:
            validate_config(reference_input)
        except ReferenceInputError as error:
            raise ControlledError(str(error)) from error
        if os.name != "nt":
            raise ControlledError("reference.input windows-messages requires Windows")

    timeout = _number_field(config, "timeoutSeconds", DEFAULT_TIMEOUT,
                            MIN_TIMEOUT, MAX_TIMEOUT, "configuration")
    checkpoint = _number_field(config, "checkpointSeconds", DEFAULT_CHECKPOINT,
                               1, timeout, "configuration")
    return {
        "privateRoot": private_root,
        "engine": {"argv": _argv_field(engine, "command", "engine"),
                   "cwd": engine_cwd, "backend": backend, "distribution": distribution},
        "reference": {"argv": _argv_field(reference, "command", "reference"),
                      "cwd": reference_cwd, "input": reference_input},
        "timeout": float(timeout),
        "checkpoint": float(checkpoint),
    }
