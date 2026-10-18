# SPDX-License-Identifier: GPL-3.0-or-later
"""Owned subprocess trees and Windows Job Object helpers for compare_game."""
import ctypes
import os
from pathlib import Path
import subprocess
import sys
import time
from compare_common import ControlledError

if os.name == "nt":
    import ctypes.wintypes
else:
    import signal


if os.name != "nt":
    class _PeekPopen(subprocess.Popen):
        """poll() reports exit without reaping the group leader. An unreaped
        zombie leader keeps its pid, and so the pgid, from being reused before
        kill_tree signals the group; only kill_tree's wait() reaps."""

        def poll(self):
            if self.returncode is not None:
                return self.returncode
            try:
                info = os.waitid(os.P_PID, self.pid,
                                 os.WEXITED | os.WNOHANG | os.WNOWAIT)
            except ChildProcessError:
                return super().poll()
            if info is None or info.si_pid == 0:
                return None
            return info.si_status if info.si_code == os.CLD_EXITED else -info.si_status


if os.name == "nt":
    CREATE_SUSPENDED = 0x00000004
    THREAD_SUSPEND_RESUME = 0x0002
    JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE = 0x00002000
    JOB_OBJECT_EXTENDED_LIMIT_INFORMATION_CLASS = 9

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

    class _BASIC_ACCOUNTING_INFO(ctypes.Structure):
        _fields_ = [
            ("TotalUserTime", ctypes.c_longlong),
            ("TotalKernelTime", ctypes.c_longlong),
            ("ThisPeriodTotalUserTime", ctypes.c_longlong),
            ("ThisPeriodTotalKernelTime", ctypes.c_longlong),
            ("TotalPageFaultCount", ctypes.wintypes.DWORD),
            ("TotalProcesses", ctypes.wintypes.DWORD),
            ("ActiveProcesses", ctypes.wintypes.DWORD),
            ("TotalTerminatedProcesses", ctypes.wintypes.DWORD)]

    # A private handle: prototypes are not shared with other modules, and
    # ctypes.get_last_error() is captured right after each call.
    _KERNEL32 = ctypes.WinDLL("kernel32", use_last_error=True)
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
    JOB_OBJECT_BASIC_ACCOUNTING_INFORMATION_CLASS = 1
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
            f"cannot enumerate child threads (error {ctypes.get_last_error()}); "
            "refusing to spawn")
    identifiers = []
    entry = _THREADENTRY32()
    entry.dwSize = ctypes.sizeof(entry)
    stepping = kernel32.Thread32First(snapshot, ctypes.byref(entry))
    if not stepping:
        kernel32.CloseHandle(ctypes.wintypes.HANDLE(snapshot))
        raise ControlledError(
            f"cannot walk the child thread list (error {ctypes.get_last_error()}); "
            "refusing to spawn")
    while stepping:
        if entry.th32OwnerProcessID == pid:
            identifiers.append(entry.th32ThreadID)
        stepping = kernel32.Thread32Next(snapshot, ctypes.byref(entry))
    if not kernel32.CloseHandle(ctypes.wintypes.HANDLE(snapshot)):
        raise ControlledError(
            f"CloseHandle failed on the thread snapshot (error {ctypes.get_last_error()})")
    return identifiers


def _job_process_count(job):
    """Number of processes in the owned job; None when unverifiable."""
    if os.name != "nt" or job is None:
        return None
    # ActiveProcesses has no list-size limit (the PID-list query fails past 256).
    box = _BASIC_ACCOUNTING_INFO()
    returned = ctypes.wintypes.DWORD()
    if not _KERNEL32.QueryInformationJobObject(
            ctypes.wintypes.HANDLE(job), JOB_OBJECT_BASIC_ACCOUNTING_INFORMATION_CLASS,
            ctypes.byref(box), ctypes.sizeof(box), ctypes.byref(returned)):
        return None
    return box.ActiveProcesses


_WSL_STEMS = frozenset({"wsl", "wslg", "wslhost", "bash"})
_WSL_DISTRO_PREFIXES = ("ubuntu", "debian", "kali", "opensuse", "suse", "sles",
                        "oraclelinux", "almalinux", "archlinux", "fedora")


def is_wsl_launcher_name(path):
    """True when the final path component names a WSL launcher or redirector.

    Name-based on purpose: Windows may resolve a bare name (bash.exe) through
    System32 before PATH, so which() cannot say what CreateProcess will start.
    Matches bash/bash.exe too, so a Git bash earlier on PATH is rejected as well
    (fail closed). Separators of either style are honored on every platform.
    """
    name = str(path).replace("\\", "/").rsplit("/", 1)[-1].strip().rstrip(" .").lower()
    stem = name[:-4] if name.endswith(".exe") else name
    return stem in _WSL_STEMS or stem.startswith(_WSL_DISTRO_PREFIXES)


def _reject_wsl(argv):
    if os.name != "nt":
        return
    if is_wsl_launcher_name(argv[0]):
        raise ControlledError(
            f"unsupported backend: {argv[0]!r} is a WSL launcher; a Job Object "
            "cannot own its Linux children")


def _suspended_image_path(process):
    """Executable image the OS actually mapped for a started process, or None."""
    size = ctypes.wintypes.DWORD(32768)
    buffer = ctypes.create_unicode_buffer(size.value)
    query = _KERNEL32.QueryFullProcessImageNameW
    query.argtypes = [ctypes.wintypes.HANDLE, ctypes.wintypes.DWORD,
                      ctypes.wintypes.LPWSTR, ctypes.POINTER(ctypes.wintypes.DWORD)]
    query.restype = ctypes.wintypes.BOOL
    if not query(ctypes.wintypes.HANDLE(_process_handle(process)), 0, buffer, ctypes.byref(size)):
        return None
    return buffer.value


def _process_handle(process):
    """Popen._handle is private CPython API (stable on Windows through 3.13);
    fail closed with a clear message if it ever disappears."""
    handle = getattr(process, "_handle", None)
    if handle is None:
        raise ControlledError("this Python no longer exposes Popen._handle; "
                              "cannot adopt the child into its job object")
    return handle


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
    def launch(cls, argv, cwd, env=None, pass_fds=()):
        if os.name == "nt":
            return cls._launch_windows(argv, cwd, env=env)
        return cls._launch_posix(argv, cwd, env=env, pass_fds=pass_fds)

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
            error = ctypes.get_last_error()
            kernel32.CloseHandle(ctypes.wintypes.HANDLE(job))
            raise ControlledError(
                f"cannot configure the session job object (error {error}); refusing to spawn")
        try:
            process = subprocess.Popen(
                argv, cwd=str(cwd), stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                stderr=subprocess.PIPE, creationflags=CREATE_SUSPENDED, env=env)
        except (OSError, ValueError) as error:
            kernel32.CloseHandle(ctypes.wintypes.HANDLE(job))
            raise ControlledError(f"cannot launch {argv[0]!r}: {error}") from error
        # From here the child exists: any exit, including an interrupt, must kill
        # it and close the job exactly once (the only abort is in the handler).
        try:
            return cls._adopt_and_resume(job, process, argv, allow_wsl)
        except BaseException:
            _abort_windows_launch(job, process, None)
            raise

    @classmethod
    def _adopt_and_resume(cls, job, process, argv, allow_wsl):
        kernel32 = _KERNEL32
        if not kernel32.AssignProcessToJobObject(
                ctypes.wintypes.HANDLE(job), ctypes.wintypes.HANDLE(_process_handle(process))):
            error = ctypes.get_last_error()
            raise ControlledError(
                f"cannot adopt {argv[0]!r} into the session job object (error {error}); killed")
        if not allow_wsl:
            # The OS-resolved image, not argv[0], decides: still suspended here.
            image = _suspended_image_path(process)
            if image is None or is_wsl_launcher_name(image):
                raise ControlledError(
                    f"unsupported backend: {argv[0]!r} resolved to "
                    f"{image or 'an unknown image'}, a WSL launcher or an image that "
                    "cannot be verified; killed before it ran")
        threads = _thread_ids_of(process.pid)
        if not threads:
            raise ControlledError(
                f"no threads were enumerated for the suspended child {argv[0]!r}; killed")
        resumed = 0
        failures = []
        for thread_id in threads:
            thread = kernel32.OpenThread(THREAD_SUSPEND_RESUME, False, thread_id)
            if not thread:
                failures.append(f"cannot open thread {thread_id} "
                                f"(error {ctypes.get_last_error()})")
                continue
            try:
                previous = kernel32.ResumeThread(ctypes.wintypes.HANDLE(thread))
                resume_error = ctypes.get_last_error()
            finally:
                if not kernel32.CloseHandle(ctypes.wintypes.HANDLE(thread)):
                    failures.append(f"CloseHandle failed on thread {thread_id} "
                                    f"(error {ctypes.get_last_error()})")
            if previous == WAS_SUSPENDED_ERROR:
                failures.append(f"ResumeThread failed for thread {thread_id} "
                                f"(error {resume_error})")
            elif previous == 0:
                continue  # Not suspended (injected by AV/EDR); ResumeThread left it alone.
            elif previous > 1:
                failures.append(f"thread {thread_id} is still suspended (count {previous})")
            else:
                resumed += 1
        if failures or resumed != 1:
            raise ControlledError(
                f"cannot resume the suspended child {argv[0]!r}; killed: "
                + "; ".join(failures or [f"{resumed} suspended threads were resumed, not 1"]))
        return cls(job, process, None)

    @classmethod
    def _launch_posix(cls, argv, cwd, env=None, pass_fds=()):
        try:
            process = _PeekPopen(
                argv, cwd=str(cwd), stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                stderr=subprocess.PIPE, start_new_session=True, env=env,
                pass_fds=pass_fds)
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
        treat a raised ControlledError as failed, non-successful cleanup.
        An interrupt leaves `killed` False so a retry really re-runs the kill;
        a verification failure is final and is reported once."""
        if self.killed:
            return
        try:
            self._kill_tree()
        except Exception:
            self.killed = True
            raise
        self.killed = True

    def _kill_tree(self):
        if os.name == "nt":
            kernel32 = _KERNEL32
            errors = []
            job = self.job  # None when an earlier, interrupted attempt already released it.
            if job is not None and not kernel32.TerminateJobObject(ctypes.wintypes.HANDLE(job), 1):
                errors.append(f"TerminateJobObject failed (error {ctypes.get_last_error()})")
            try:
                self.process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                errors.append("owned leader still alive after TerminateJobObject")
            remaining = -1 if job is not None else 0
            for _ in range(10 if job is not None else 0):
                remaining = _job_process_count(job)
                if remaining == 0:
                    break
                time.sleep(0.1)
            if remaining != 0:
                errors.append(f"owned job members may still be alive ({remaining!r})")
            self.job = None  # Cleared before the close: a retry must never close it twice.
            if job is not None and not kernel32.CloseHandle(ctypes.wintypes.HANDLE(job)):
                errors.append(f"CloseHandle failed (error {ctypes.get_last_error()})")
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
