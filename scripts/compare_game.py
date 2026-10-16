#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Bounded single-comparison preview controller; unpaired/inconclusive slice.

Runs one engine JSONL harness session beside one reference process, optionally
mirrors mapped input to an owned SDL window, and never claims parity. See README.md
for the version-1 session configuration and its prerequisites.
"""
import ctypes
import base64
import json
import math
import os
from pathlib import Path, PurePosixPath
import queue
import shutil
import stat
import subprocess
import sys
import threading
import time
import uuid
from reference_input import ReferenceInputError, WindowsMessages, key_sequence, validate_config

if os.name == "nt":
    import ctypes.wintypes
    import msvcrt
else:
    import fcntl
    import signal

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


def _split_resolved(path):
    return os.path.normcase(str(Path(path).resolve()))


def _contains(parent, child):
    parent, child = _split_resolved(parent), _split_resolved(child)
    prefix = parent if parent.endswith(os.sep) else parent + os.sep
    return child == parent or child.startswith(prefix)


def _outside_repository(path):
    repository = _split_resolved(Path(__file__).resolve().parent.parent)
    child = _split_resolved(path)
    if os.name == "nt" and child.split(":")[0] != repository.split(":")[0]:
        return True
    return child != repository and not child.startswith(repository + os.sep)


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


class SessionLock:
    """Advisory single-session lock held for the whole controller lifetime.

    No stale-lock deletion: OS lock semantics release the lock when this
    process exits, including after abnormal termination.
    """

    def __init__(self, private_root: Path):
        self.path = private_root / "goldbox-compare.lock"
        self.fd = None

    def acquire(self):
        if os.path.lexists(self.path):
            if os.path.islink(self.path):
                raise ControlledError(
                    f"session lock {self.path} is a symlink; refusing to open")
            if not stat.S_ISREG(os.lstat(self.path).st_mode):
                raise ControlledError(
                    f"session lock {self.path} is not a regular file; refusing to open")
        try:
            self.fd = os.open(self.path, os.O_RDWR | os.O_CREAT, 0o600)
        except OSError as error:
            self.fd = None
            raise ControlledError(f"cannot open the session lock {self.path}: {error}") from error
        try:
            size = os.fstat(self.fd).st_size
            if size == 0:
                os.write(self.fd, b"0")
            elif size == 1:
                marker = os.read(self.fd, 1)
                if marker not in (b"0", b""):
                    raise ControlledError(
                        f"session lock {self.path} holds unexpected content; refusing")
            else:
                raise ControlledError(
                    f"session lock {self.path} holds unexpected content; refusing")
            os.lseek(self.fd, 0, os.SEEK_SET)
        except (OSError, ControlledError) as error:
            os.close(self.fd)
            self.fd = None
            raise ControlledError(f"cannot prepare the session lock {self.path}: {error}") from error
        try:
            if os.name == "nt":
                msvcrt.locking(self.fd, msvcrt.LK_NBLCK, 1)
            else:
                fcntl.flock(self.fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except OSError as error:
            os.close(self.fd)
            self.fd = None
            raise ControlledError(
                f"another comparison already holds the session lock {self.path}: {error}"
            ) from error
        return self

    def release(self):
        if self.fd is None:
            return
        try:
            try:
                os.lseek(self.fd, 0, os.SEEK_SET)
                if os.name == "nt":
                    msvcrt.locking(self.fd, msvcrt.LK_UNLCK, 1)
                else:
                    fcntl.flock(self.fd, fcntl.LOCK_UN)
            except OSError:
                pass
        finally:
            os.close(self.fd)
            self.fd = None


if os.name == "nt":
    CREATE_SUSPENDED = 0x00000004
    THREAD_SUSPEND_RESUME = 0x0002
    JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE = 0x00002000
    JOB_OBJECT_EXTENDED_LIMIT_INFORMATION_CLASS = 9
    WSL_BASENAMES = frozenset({"wsl.exe", "wsl", "wslg.exe", "wslhost.exe"})

    class _IO_COUNTERS(ctypes.Structure):
        _fields_ = [(name, ctypes.c_ulonglong) for name in
                    ("ReadOperationCount", "WriteOperationCount", "OtherOperationCount",
                     "ReadTransferCount", "WriteTransferCount", "OtherTransferCount")]

    class _BASIC_LIMIT_INFO(ctypes.Structure):
        _fields_ = [
            ("PerProcessUserTimeLimit", ctypes.c_longlong),
            ("PerJobUserTimeLimit", ctypes.c_longlong),
            ("LimitFlags", ctypes.c_uint32),
            ("MinimumWorkingSetSize", ctypes.c_size_t),
            ("MaximumWorkingSetSize", ctypes.c_size_t),
            ("ActiveProcessLimit", ctypes.c_uint32),
            ("Affinity", ctypes.c_size_t),
            ("PriorityClass", ctypes.c_uint32),
            ("SchedulingClass", ctypes.c_uint32)]

    class _EXTENDED_LIMIT_INFO(ctypes.Structure):
        _fields_ = [
            ("BasicLimitInformation", _BASIC_LIMIT_INFO),
            ("IoInfo", _IO_COUNTERS),
            ("ProcessMemoryLimit", ctypes.c_size_t),
            ("JobMemoryLimit", ctypes.c_size_t),
            ("PeakProcessMemoryUsed", ctypes.c_size_t),
            ("PeakJobMemoryUsed", ctypes.c_size_t)]

    class _THREADENTRY32(ctypes.Structure):
        _fields_ = [
            ("dwSize", ctypes.wintypes.DWORD),
            ("cntUsage", ctypes.wintypes.DWORD),
            ("th32ThreadID", ctypes.wintypes.DWORD),
            ("th32OwnerProcessID", ctypes.wintypes.DWORD),
            ("tpBasePri", ctypes.wintypes.LONG),
            ("tpDeltaPri", ctypes.wintypes.LONG),
            ("dwFlags", ctypes.wintypes.DWORD)]

    class _PROCESS_ID_LIST(ctypes.Structure):
        _fields_ = [
            ("NumberOfAssignedProcesses", ctypes.wintypes.DWORD),
            ("NumberOfProcessIdsInList", ctypes.wintypes.DWORD),
            ("ProcessIdList", ctypes.c_size_t * 256)]

    _KERNEL32 = ctypes.windll.kernel32
    _KERNEL32.CreateJobObjectW.restype = ctypes.wintypes.HANDLE
    _KERNEL32.CreateJobObjectW.argtypes = [ctypes.wintypes.LPVOID, ctypes.wintypes.LPCWSTR]
    _KERNEL32.SetInformationJobObject.restype = ctypes.wintypes.BOOL
    _KERNEL32.SetInformationJobObject.argtypes = [
        ctypes.wintypes.HANDLE, ctypes.wintypes.DWORD, ctypes.wintypes.LPVOID,
        ctypes.wintypes.DWORD]
    _KERNEL32.AssignProcessToJobObject.restype = ctypes.wintypes.BOOL
    _KERNEL32.AssignProcessToJobObject.argtypes = [
        ctypes.wintypes.HANDLE, ctypes.wintypes.HANDLE]
    _KERNEL32.TerminateJobObject.restype = ctypes.wintypes.BOOL
    _KERNEL32.TerminateJobObject.argtypes = [ctypes.wintypes.HANDLE, ctypes.wintypes.UINT]
    _KERNEL32.CloseHandle.restype = ctypes.wintypes.BOOL
    _KERNEL32.CloseHandle.argtypes = [ctypes.wintypes.HANDLE]
    _KERNEL32.OpenThread.restype = ctypes.wintypes.HANDLE
    _KERNEL32.OpenThread.argtypes = [ctypes.wintypes.DWORD, ctypes.wintypes.BOOL,
                                     ctypes.wintypes.DWORD]
    _KERNEL32.ResumeThread.restype = ctypes.wintypes.DWORD
    _KERNEL32.ResumeThread.argtypes = [ctypes.wintypes.HANDLE]
    _KERNEL32.CreateToolhelp32Snapshot.restype = ctypes.wintypes.HANDLE
    _KERNEL32.CreateToolhelp32Snapshot.argtypes = [ctypes.wintypes.DWORD, ctypes.wintypes.DWORD]
    _KERNEL32.Thread32First.restype = ctypes.wintypes.BOOL
    _KERNEL32.Thread32First.argtypes = [ctypes.wintypes.HANDLE,
                                        ctypes.POINTER(_THREADENTRY32)]
    _KERNEL32.Thread32Next.restype = ctypes.wintypes.BOOL
    _KERNEL32.Thread32Next.argtypes = [ctypes.wintypes.HANDLE,
                                       ctypes.POINTER(_THREADENTRY32)]
    _KERNEL32.QueryInformationJobObject.restype = ctypes.wintypes.BOOL
    _KERNEL32.QueryInformationJobObject.argtypes = [
        ctypes.wintypes.HANDLE, ctypes.wintypes.DWORD, ctypes.wintypes.LPVOID,
        ctypes.wintypes.DWORD, ctypes.POINTER(ctypes.wintypes.DWORD)]

    TH32CS_SNAPTHREAD = 0x00000004
    JOB_OBJECT_BASIC_PROCESS_ID_LIST_CLASS = 3
    THREAD_STILL_ACTIVE = 259
    WAS_SUSPENDED_ERROR = 0xFFFFFFFF
    INVALID_HANDLE_VALUE = ctypes.c_void_p(-1).value


def _thread_ids_of(pid):
    """Enumerate the threads of a suspended child; never trust a spawn pid."""
    if os.name != "nt":
        return []
    kernel32 = _KERNEL32
    snapshot = kernel32.CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0)
    if snapshot is None or snapshot == INVALID_HANDLE_VALUE:
        raise ControlledError(
            f"cannot enumerate child threads (error {kernel32.GetLastError()}); "
            "refusing to spawn")
    identifiers = []
    entry = _THREADENTRY32()
    entry.dwSize = ctypes.sizeof(entry)
    stepping = kernel32.Thread32First(snapshot, ctypes.byref(entry))
    if not stepping:
        kernel32.CloseHandle(ctypes.wintypes.HANDLE(snapshot))
        raise ControlledError(
            f"cannot walk the child thread list (error {kernel32.GetLastError()}); "
            "refusing to spawn")
    while stepping:
        if entry.th32OwnerProcessID == pid:
            identifiers.append(entry.th32ThreadID)
        stepping = kernel32.Thread32Next(snapshot, ctypes.byref(entry))
    if not kernel32.CloseHandle(ctypes.wintypes.HANDLE(snapshot)):
        raise ControlledError(
            f"CloseHandle failed on the thread snapshot (error {kernel32.GetLastError()})")
    return identifiers


def _job_process_count(job):
    """Number of processes in the owned job; None when unverifiable."""
    if os.name != "nt" or job is None:
        return None
    box = _PROCESS_ID_LIST()
    returned = ctypes.wintypes.DWORD()
    if not _KERNEL32.QueryInformationJobObject(
            ctypes.wintypes.HANDLE(job), JOB_OBJECT_BASIC_PROCESS_ID_LIST_CLASS,
            ctypes.byref(box), ctypes.sizeof(box), ctypes.byref(returned)):
        return None
    return box.NumberOfAssignedProcesses


def _reject_wsl(argv):
    if os.name != "nt":
        return
    basename = os.path.basename(argv[0]).lower()
    if basename in WSL_BASENAMES:
        raise ControlledError(
            f"unsupported backend: {argv[0]!r} is a WSL launcher; a Job Object "
            "cannot own its Linux children")
    if basename == "bash.exe":
        resolved = shutil.which(argv[0])
        if resolved:
            redirector = os.path.join(os.path.expandvars(r"%SystemRoot%"),
                                      "System32", "bash.exe")
            if os.path.normcase(resolved) == os.path.normcase(redirector):
                raise ControlledError(
                    f"unsupported backend: {resolved} is the WSL bash redirector; this "
                    "controller does not own its Linux children")


def _abort_windows_launch(job, process, message):
    """Terminate everything owned by a failed launch and close the job."""
    kernel32 = _KERNEL32
    kernel32.TerminateJobObject(ctypes.wintypes.HANDLE(job), 1)
    try:
        # Assignment may have failed while the child was still suspended.
        if process.poll() is None:
            process.kill()
        process.wait(timeout=10)
    except subprocess.TimeoutExpired:
        pass
    kernel32.CloseHandle(ctypes.wintypes.HANDLE(job))
    if message:
        raise ControlledError(message)


class OwnedProcess:
    """A subprocess whose whole tree this controller owns and terminates."""

    def __init__(self, job, process, posix_group):
        self.job = job
        self.process = process
        self.posix_group = posix_group
        self.killed = False

    @classmethod
    def launch(cls, argv, cwd, env=None):
        if os.name == "nt":
            return cls._launch_windows(argv, cwd, env=env)
        return cls._launch_posix(argv, cwd, env=env)

    @classmethod
    def _launch_windows(cls, argv, cwd, allow_wsl=False, env=None):
        if not allow_wsl:
            _reject_wsl(argv)
        kernel32 = _KERNEL32
        job = kernel32.CreateJobObjectW(None, None)
        if not job:
            raise ControlledError(
                "cannot create the session job object; refusing to spawn an unowned child")
        limits = _EXTENDED_LIMIT_INFO()
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE
        if not kernel32.SetInformationJobObject(
                ctypes.wintypes.HANDLE(job), JOB_OBJECT_EXTENDED_LIMIT_INFORMATION_CLASS,
                ctypes.byref(limits), ctypes.sizeof(limits)):
            error = kernel32.GetLastError()
            kernel32.CloseHandle(ctypes.wintypes.HANDLE(job))
            raise ControlledError(
                f"cannot configure the session job object (error {error}); refusing to spawn")
        try:
            process = subprocess.Popen(
                argv, cwd=str(cwd), stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                stderr=subprocess.PIPE, creationflags=CREATE_SUSPENDED, env=env)
        except OSError as error:
            kernel32.CloseHandle(ctypes.wintypes.HANDLE(job))
            raise ControlledError(f"cannot launch {argv[0]!r}: {error}") from error
        if not kernel32.AssignProcessToJobObject(
                ctypes.wintypes.HANDLE(job), ctypes.wintypes.HANDLE(process._handle)):
            error = kernel32.GetLastError()
            _abort_windows_launch(job, process, None)
            raise ControlledError(
                f"cannot adopt {argv[0]!r} into the session job object (error {error}); killed")
        try:
            threads = _thread_ids_of(process.pid)
        except BaseException:
            _abort_windows_launch(job, process, None)
            raise
        if not threads:
            _abort_windows_launch(job, process, None)
            raise ControlledError(
                f"no threads were enumerated for the suspended child {argv[0]!r}; killed")
        resumed = 0
        failures = []
        for thread_id in threads:
            thread = kernel32.OpenThread(THREAD_SUSPEND_RESUME, False, thread_id)
            if not thread:
                failures.append(f"cannot open thread {thread_id} "
                                f"(error {kernel32.GetLastError()})")
                continue
            try:
                previous = kernel32.ResumeThread(ctypes.wintypes.HANDLE(thread))
            finally:
                if not kernel32.CloseHandle(ctypes.wintypes.HANDLE(thread)):
                    failures.append(f"CloseHandle failed on thread {thread_id} "
                                    f"(error {kernel32.GetLastError()})")
            if previous == WAS_SUSPENDED_ERROR:
                failures.append(f"ResumeThread failed for thread {thread_id} "
                                f"(error {kernel32.GetLastError()})")
            elif previous < 1:
                failures.append(f"thread {thread_id} was not suspended as expected")
            else:
                resumed += 1
        if failures or not resumed:
            _abort_windows_launch(job, process, None)
            raise ControlledError(
                f"cannot resume the suspended child {argv[0]!r}; killed: "
                + "; ".join(failures or ["no thread was resumed"]))
        return cls(job, process, None)

    @classmethod
    def _launch_posix(cls, argv, cwd, env=None):
        try:
            process = subprocess.Popen(
                argv, cwd=str(cwd), stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                stderr=subprocess.PIPE, start_new_session=True, env=env)
        except OSError as error:
            raise ControlledError(f"cannot launch {argv[0]!r}: {error}") from error
        return cls(None, process, process.pid)

    def _close_stdin(self):
        try:
            self.process.stdin.close()
        except OSError:
            pass

    def kill_tree(self):
        """Kill the owned tree and verify members are gone; the caller must
        treat a raised ControlledError as failed, non-successful cleanup."""
        if self.killed:
            return
        self.killed = True
        if os.name == "nt":
            kernel32 = _KERNEL32
            errors = []
            if not kernel32.TerminateJobObject(ctypes.wintypes.HANDLE(self.job), 1):
                errors.append(f"TerminateJobObject failed (error {kernel32.GetLastError()})")
            try:
                self.process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                errors.append("owned leader still alive after TerminateJobObject")
            remaining = -1
            for _ in range(10):
                remaining = _job_process_count(self.job)
                if remaining == 0:
                    break
                time.sleep(0.1)
            if remaining != 0:
                errors.append(f"owned job members may still be alive ({remaining!r})")
            if not kernel32.CloseHandle(ctypes.wintypes.HANDLE(self.job)):
                errors.append(f"CloseHandle failed (error {kernel32.GetLastError()})")
            self.job = None
            self._close_stdin()
            if errors:
                raise ControlledError("cleanup verification failed: " + "; ".join(errors))
            return
        try:
            os.killpg(self.posix_group, signal.SIGKILL)
        except ProcessLookupError:
            pass
        except OSError as error:
            raise ControlledError(f"cannot signal the owned process group: {error}")
        try:
            self.process.wait(timeout=10)
        except subprocess.TimeoutExpired as error:
            raise ControlledError(
                f"owned leader still alive after SIGKILL: {error}") from error
        self._close_stdin()
        for _ in range(20):
            if sys.platform.startswith("linux"):
                # Orphan zombies await the OS reaper but cannot execute or hold pipes.
                running = False
                for entry in Path("/proc").iterdir():
                    if not entry.name.isdecimal():
                        continue
                    try:
                        fields = (entry / "stat").read_text().rsplit(")", 1)[1].split()
                        if int(fields[2]) == self.posix_group and fields[0] != "Z":
                            running = True
                            break
                    except (FileNotFoundError, ProcessLookupError):
                        continue
                if not running:
                    return
            try:
                os.killpg(self.posix_group, 0)
            except ProcessLookupError:
                return
            except OSError as error:
                raise ControlledError(
                    f"cannot verify the owned process group: {error}") from error
            time.sleep(0.1)
        raise ControlledError("owned process group members may still be alive")


class WslEngine:
    """Owned Windows launcher plus an independently bounded Linux supervisor."""

    def __init__(self, native, config, duration):
        self.native = native
        self.process = native.process
        self.pipe = self.process.stdin
        self.process.stdin = self
        self.nonce = uuid.uuid4().hex
        self.frames = queue.Queue(maxsize=64)
        self.failure = None
        self.clock = None
        self.received = None
        self.last_renew = 0.0
        self.closed = threading.Event()
        self.clean = False
        self.stopping = False
        self.writer = threading.Thread(target=self._write_frames, daemon=True)
        self._enqueue({"op": "start", "nonce": self.nonce, "command": config["argv"],
                       "cwd": config["cwd"], "timeout": duration})
        self.writer.start()

    @classmethod
    def launch(cls, config, duration):
        source = Path(__file__).with_name("wsl_engine.py").read_text(encoding="utf-8")
        # Pass source directly to Python; no shell expansion, path guessing,
        # helper deployment, service installation or network listener is needed.
        argv = ["wsl.exe", "--distribution", config["distribution"], "--exec",
                "/usr/bin/python3", "-u", "-c", source]
        native = OwnedProcess._launch_windows(argv, Path(__file__).resolve().parent, allow_wsl=True)
        try:
            return cls(native, config, duration)
        except BaseException:
            native.kill_tree()
            raise

    def _enqueue(self, frame):
        data = (json.dumps(frame, allow_nan=False) + "\n").encode()
        if len(data) > 128 * 1024:
            raise ControlledError("WSL adapter input exceeded its bounded frame size")
        try:
            self.frames.put_nowait(data)
        except queue.Full as error:
            raise ControlledError("WSL adapter input queue exceeded its bound") from error

    def _write_frames(self):
        while True:
            data = self.frames.get()
            if data is None:
                return
            try:
                remaining = memoryview(data)
                while remaining:
                    count = os.write(self.pipe.fileno(), remaining)
                    remaining = remaining[count:]
            except (OSError, ValueError) as error:
                self.failure = ControlledError(f"WSL adapter input failed: {error}")
                return

    def write(self, data):
        self._enqueue({"op": "input", "data": base64.b64encode(data).decode("ascii")})
        return len(data)

    def flush(self):
        if self.failure is not None:
            raise self.failure

    def close(self):
        # OwnedProcess closes this proxy only after killing the Windows job.
        pass

    def heartbeat(self):
        if self.failure is not None:
            raise self.failure
        now = time.monotonic()
        if self.clock is not None and not self.closed.is_set() and now - self.last_renew >= 0.5:
            # An absolute Linux-clock expiry prevents buffered, stale heartbeats
            # from extending a lease after the Windows controller has stalled.
            until = self.clock + (now - self.received) + 5.0
            self._enqueue({"op": "renew", "until": until})
            self.last_renew = now

    def filter_event(self, document):
        if document.get("type") != "_goldbox_wsl":
            return False
        if document.get("nonce") != self.nonce:
            raise ControlledError("WSL adapter session identity mismatch")
        phase = document.get("phase")
        if phase == "ready" and self.clock is None:
            clock = document.get("clock")
            if isinstance(clock, bool) or not isinstance(clock, (int, float)) or not math.isfinite(clock):
                raise ControlledError("WSL adapter supplied an invalid clock")
            self.received = time.monotonic()
            self.clock = clock  # Publish only after the reader's clock anchor is complete.
        elif phase == "closed" and self.clock is not None and not self.closed.is_set():
            self.clean = document.get("clean") is True
            self.closed.set()
            if not self.clean:
                raise ControlledError("WSL adapter could not verify Linux cleanup")
            engine_exit = document.get("engine_exit")
            if not self.stopping and (document.get("reason") != "engine exited"
                    or isinstance(engine_exit, bool) or not isinstance(engine_exit, int) or engine_exit != 0):
                raise ControlledError(f"WSL engine stopped: {document.get('reason')}; exit {engine_exit}")
        else:
            raise ControlledError("WSL adapter control event is out of order")
        return True

    def kill_tree(self):
        self.stopping = True
        errors = []
        try:
            if self.process.poll() is None:
                try:
                    self._enqueue({"op": "stop"})
                except ControlledError:
                    pass  # The independent Linux lease is still enforced.
                try:
                    self.process.wait(timeout=8)
                except subprocess.TimeoutExpired:
                    errors.append("WSL adapter did not exit within its cleanup budget")
            if not self.closed.wait(timeout=0.5) or not self.clean:
                errors.append("Linux cleanup acknowledgment was not received")
        finally:
            try:
                self.native.kill_tree()
            finally:
                self.pipe.close()
                try:
                    self.frames.put_nowait(None)
                except queue.Full:
                    pass
                self.writer.join(timeout=1)
        if errors:
            raise ControlledError("; ".join(errors))


class ConsoleInput(threading.Thread):
    """Bounded operator stdin reader; oversized or flooded input fails the
    session rather than being silently discarded."""

    def __init__(self):
        super().__init__(daemon=True)
        self.queue = queue.Queue(maxsize=OPERATOR_QUEUE_LIMIT)
        self.failure = None

    def run(self):
        try:
            buffer = bytearray()
            while self.failure is None:
                try:
                    # Avoid holding Python's buffered stdin lock at interpreter shutdown.
                    chunk = os.read(sys.stdin.fileno(), 4096)
                except (OSError, ValueError):
                    break
                if not chunk:
                    break
                buffer.extend(chunk)
                while b"\n" in buffer and self.failure is None:
                    line, _, rest = bytes(buffer).partition(b"\n")
                    buffer = bytearray(rest)
                    self._offer(line)
                if len(buffer) > OPERATOR_LINE_LIMIT:
                    self.failure = FloodError("operator input exceeds its bounded line limit")
            if self.failure is None:
                if len(buffer) > OPERATOR_LINE_LIMIT:
                    self.failure = ControlledError(
                        f"operator input exceeds the {OPERATOR_LINE_LIMIT} byte "
                        "line bound")
                elif buffer:
                    self._offer(bytes(buffer))
        except Exception as error:  # defensive: the reader must not exit silently
            self.failure = ControlledError(f"operator console reader failed: {error}")
        try:
            self.queue.put_nowait(None)
        except queue.Full:
            pass

    def _offer(self, raw):
        if len(raw) > OPERATOR_LINE_LIMIT:
            self.failure = ControlledError(
                f"operator line exceeds the {OPERATOR_LINE_LIMIT} byte bound")
            return
        text = raw.decode("utf-8", errors="replace").strip()
        try:
            self.queue.put_nowait(text)
        except queue.Full:
            self.failure = FloodError(
                "operator input queue exceeded its bounded limit")


class _BoundedDrain(threading.Thread):
    """Bounded drain for stderr-style pipes; surfaces flood/error failures."""

    def __init__(self, stream, what, limit):
        super().__init__(daemon=True)
        self.stream = stream
        self.what = what
        self.limit = limit
        self.total = 0
        self.failure = None

    def run(self):
        try:
            while True:
                try:
                    chunk = self.stream.read1(CHUNK)
                except OSError:
                    return
                if not chunk:
                    return
                self.total += len(chunk)
                if self.total > self.limit:
                    self.failure = FloodError(
                        f"{self.what} exceeded the bounded drain limit ({self.limit} bytes)")
                    return
        except Exception as error:  # defensive: the drain must not exit silently
            self.failure = ControlledError(f"{self.what} drain failed: {error}")


class _EventQueue:
    def __init__(self):
        self.queue = queue.Queue(maxsize=EVENT_QUEUE_LIMIT)
        self.failure = None
        self.reader = None

    def put(self, item):
        try:
            self.queue.put_nowait(item)
        except queue.Full:
            self.mark_failure(FloodError("engine event queue exceeded its bounded limit"))
            return False
        return True

    def mark_failure(self, failure):
        if self.failure is None:
            self.failure = failure


class EngineStdoutReader(threading.Thread):
    """Bounded JSONL reader: line-too-long and flood become failures."""

    def __init__(self, stream, queue_bound, filter_event=None):
        super().__init__(daemon=True)
        self.stream = stream
        self.queue = queue_bound
        self.total = 0
        self.filter_event = filter_event

    def run(self):
        buffer = bytearray()
        try:
            while True:
                try:
                    chunk = self.stream.read1(CHUNK)
                except OSError:
                    break
                if not chunk:
                    break
                self.total += len(chunk)
                if self.total > ENGINE_OUTPUT_LIMIT:
                    self.queue.mark_failure(FloodError(
                        f"engine stdout exceeded its bounded capture limit "
                        f"({ENGINE_OUTPUT_LIMIT} bytes)"))
                    return
                buffer.extend(chunk)
                if b"\n" not in buffer and len(buffer) > ENGINE_LINE_LIMIT:
                    self.queue.mark_failure(FloodError(
                        f"engine line exceeds the {ENGINE_LINE_LIMIT} byte bound "
                        "before any newline"))
                    return
                while b"\n" in buffer:
                    line, _, rest = bytes(buffer).partition(b"\n")
                    buffer = bytearray(rest)
                    if len(line) > ENGINE_LINE_LIMIT:
                        self.queue.mark_failure(FloodError(
                            f"engine line longer than the {ENGINE_LINE_LIMIT} byte bound"))
                        return
                    if not self._offer(line):
                        return
            if buffer:
                self._offer(bytes(buffer))
            self.queue.put(("eof", None))
        except Exception as error:  # defensive: the reader must not exit silently
            self.queue.mark_failure(ControlledError(f"engine reader failed: {error}"))
            self.queue.put(("eof", None))

    def _offer(self, line):
        try:
            document = json.loads(line.decode("utf-8"))
        except ValueError as error:
            self.queue.mark_failure(
                ControlledError(f"engine emitted invalid JSON: {error}"))
            self.queue.put(("eof", None))
            return False
        if not isinstance(document, dict):
            self.queue.mark_failure(
                ControlledError("engine emitted a non-object JSON document"))
            self.queue.put(("eof", None))
            return False
        if self.filter_event is not None and self.filter_event(document):
            return True
        return self.queue.put(("event", document))


USAGE = "usage: compare_game.py <session-config.json>"


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
        value = document.get(field)
        if isinstance(value, bool) or not isinstance(value, int) or value < 0:
            _reject(value, f"engine ready {field}")
        parts.append(f"{field} {value}")
    print("[engine ready]")
    print(f"  protocol 1; source_identity={identity[:64]}")
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
        print(text)


def _offer_menu(document):
    choices = document.get("choices")
    if not isinstance(choices, list) or not choices \
            or any(not isinstance(item, str) for item in choices):
        _reject(choices, "engine menu choices")
    prompt = document.get("prompt")
    if not isinstance(prompt, str):
        _reject(prompt, "engine menu prompt")
    if not isinstance(document.get("vertical"), bool):
        _reject(document.get("vertical"), "engine menu vertical flag")
    if prompt:
        print(prompt)
    print("menus accept a zero-based choice number shown alongside each entry.")
    for index, choice in enumerate(choices):
        print(f"  [{index}] {choice}")
    return choices


def _render_complete(document):
    status = document.get("status")
    if not isinstance(status, str) or status not in ("ok", "blocked", "unsupported"):
        _reject(status, "engine complete status")
    state = document.get("state")
    if not isinstance(state, dict):
        _reject(state, "engine complete state")
    for name in ("row", "column", "facing"):
        value = document.get(name)
        if isinstance(value, bool) or not isinstance(value, int) or value < 0:
            _reject(value, f"engine complete {name}")
    position = " ".join(f"{name}={document.get(name)!r}" for name in ("row", "column", "facing"))
    print(f"[complete id={document.get('id')!r}] status={status}; {position}")
    dispatches = document.get("dispatches")
    if not isinstance(dispatches, list):
        _reject(dispatches, "engine complete dispatches")
    for dispatch in dispatches:
        if not isinstance(dispatch, dict):
            _reject(dispatch, "engine dispatch entry")
        for name in ("table_offset", "selector", "target_offset"):
            value = dispatch.get(name)
            if isinstance(value, bool) or not isinstance(value, int) or value < 0:
                _reject(value, f"engine dispatch {name}")
        if not isinstance(dispatch.get("subroutine"), bool):
            _reject(dispatch.get("subroutine"), "engine dispatch subroutine flag")
        print("  dispatch: " + ", ".join(
            f"{name}={dispatch.get(name)!r}" for name in
            ("table_offset", "selector", "target_offset", "subroutine")))
    diagnostic = document.get("diagnostic")
    if not isinstance(diagnostic, str):
        _reject(diagnostic, "engine complete diagnostic")
    for name in ("steps", "bytecode_offset"):
        value = document.get(name)
        if isinstance(value, bool) or not isinstance(value, int) or value < 0:
            _reject(value, f"engine complete {name}")
    if status == "unsupported":
        print(f"  unsupported diagnostic: {diagnostic}")
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
        try:
            self.engine.process.wait(timeout=budget)
        except subprocess.TimeoutExpired as error:
            raise ControlledError(
                "engine did not exit after acknowledging quit") from error
        code = self.engine.process.returncode
        if code != 0:
            raise ControlledError(
                f"engine exited with code {code} after acknowledging quit")
        deadline = time.monotonic() + budget
        while True:
            self._check_housekeeping()
            leftover = deadline - time.monotonic()
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
                self.reference_input.window()
                self.armed = True
                print("[dual input] armed; action/menu keys go to both sides. Baseline remains unverified.")
            elif verb == "disarm":
                self.armed = False
                print("[dual input] disarmed")
            else:
                if self.armed:
                    raise ControlledError("disarm before sending reference-only staging keys")
                keys = key_sequence(item.upper().split()[1:])
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
            print(f"[ignored] unknown command {item!r}; type 'help' for the accepted list")
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

        actions = [write] if document is not None else []
        if reference_keys is not None:
            actions.append(lambda: self.reference_input.send(reference_keys, limit))
        threads = [threading.Thread(target=worker, args=(action,), daemon=True) for action in actions]
        for thread in threads:
            thread.start()
        gate.set()
        while any(thread.is_alive() for thread in threads):
            self._check_housekeeping()
            if time.monotonic() >= limit:
                raise ControlledError("input dispatch exceeded its bounded deadline; delivery may be partial")
            time.sleep(0.01)
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
            print("[ignored] menu choice requires a decimal number; re-enter it")
            return
        if not 0 <= choice < len(choices):
            print(f"[ignored] menu choice {choice} lies outside the offered range "
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
            self.last_summary = f"text (action {event_id})"
            return
        if event_type == "menu":
            if self.pending_menu is not None:
                raise ControlledError(
                    "engine sent a second menu before the pending menu was answered")
            choices = _offer_menu(document)
            menu_id = document.get("menu_id")
            if isinstance(menu_id, bool) or not isinstance(menu_id, int) or menu_id < 1:
                _reject(menu_id, "engine menu_id")
            if menu_id <= self.last_menu_id:
                raise ControlledError(
                    f"engine menu_id {menu_id} is not strictly greater than the "
                    f"previous menu id {self.last_menu_id}")
            self.last_menu_id = menu_id
            self.pending_menu = (self.pending_id, menu_id, choices)
            self.last_summary = f"menu event (action {event_id}, menu {menu_id})"
            return
        if event_type == "complete":
            _render_complete(document)
            self.facing = document["facing"]
            if self.reference_input is not None and document["status"] == "unsupported":
                raise ControlledError("engine action unsupported; dual input stopped before another action")
            if self.quit_requested and document["status"] != "ok":
                raise ControlledError("engine did not successfully acknowledge quit")
            self.last_summary = f"complete status {document.get('status')!r} (id {event_id})"
            self.pending_id = None
            return
        raise ControlledError(f"unknown engine event type {event_type!r}")


def _wait_bounded(thread):
    if thread is not None and thread.is_alive():
        try:
            thread.join(timeout=5.0)
        except RuntimeError:
            pass


def run_session(config):
    lock = SessionLock(config["privateRoot"]).acquire()
    engine = reference = None
    stdout_reader = stderr_reader = None
    ref_drains = []
    session = None
    deadline = time.monotonic() + config["timeout"]
    try:
        if config["engine"].get("backend") == "wsl":
            engine = WslEngine.launch(config["engine"], config["timeout"])
        else:
            engine = OwnedProcess.launch(config["engine"]["argv"], config["engine"]["cwd"])
        engine_queue = _EventQueue()
        stdout_reader = EngineStdoutReader(engine.process.stdout, engine_queue,
            engine.filter_event if isinstance(engine, WslEngine) else None)
        engine_queue.reader = stdout_reader
        stdout_reader.start()
        stderr_reader = _BoundedDrain(engine.process.stderr, "engine stderr",
                                      REFERENCE_OUTPUT_LIMIT)
        stderr_reader.start()
        reference_env = None
        if config["reference"].get("input") is not None:
            reference_env = dict(os.environ, SDL_VIDEODRIVER="windib")
        reference = OwnedProcess.launch(config["reference"]["argv"],
                                        config["reference"]["cwd"], env=reference_env)
        ref_drains = [_BoundedDrain(reference.process.stdout, "reference stdout",
                                    REFERENCE_OUTPUT_LIMIT),
                      _BoundedDrain(reference.process.stderr, "reference stderr",
                                    REFERENCE_OUTPUT_LIMIT)]
        for drain in ref_drains:
            drain.start()
        console = ConsoleInput()
        console.start()
        session = Session(config, engine, reference, engine_queue, console, deadline)
        session.drain_watch = [stderr_reader, *ref_drains]
        code = session.run()
        _wait_bounded(stdout_reader)
        if engine_queue.failure is not None:
            raise engine_queue.failure
        if console.failure is not None:
            raise console.failure
        if code != 0:
            raise ControlledError(f"session returned an unexpected status {code!r}")
        reference_exit = reference.process.poll()
        if reference_exit is not None and reference_exit != 0:
            raise ControlledError(
                f"reference exited with code {reference_exit}; session inconclusive "
                "(no comparison result)")
        for drain in ref_drains + [stderr_reader]:
            if drain.failure is not None:
                raise drain.failure
        notes = session.end_notes
        return code, notes
    finally:
        cleanup_errors = []
        for owned in (engine, reference):
            if owned is not None:
                try:
                    owned.kill_tree()
                except Exception as error:
                    cleanup_errors.append(str(error))
        _wait_bounded(stdout_reader)
        _wait_bounded(stderr_reader)
        for drain in ref_drains:
            _wait_bounded(drain)
        lock.release()
        if cleanup_errors:
            raise ControlledError("cleanup failed: " + "; ".join(cleanup_errors))


def _announce(notes):
    for note in notes:
        print(note)


def main(argv):
    if len(argv) != 2:
        print(USAGE, file=sys.stderr)
        return 2
    try:
        config = parse_config(Path(argv[1]))
        code, notes = run_session(config)
        _announce(notes)
        return code
    except ControlledError as error:
        print(f"compare_game: {error}", file=sys.stderr)
        return 1
    except ReferenceInputError as error:
        print(f"compare_game: {error}", file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        print("compare_game: cancelled by operator", file=sys.stderr)
        return 1
    except Exception as error:  # fail closed rather than claim success
        print(f"compare_game: unexpected failure: {error!r}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
