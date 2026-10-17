# SPDX-License-Identifier: GPL-3.0-or-later
"""Advisory single-session lock for the compare_game controller."""
import os
from pathlib import Path
import stat
from compare_common import ControlledError

if os.name == "nt":
    import msvcrt
else:
    import fcntl


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
            self.fd = os.open(self.path, os.O_RDWR | os.O_CREAT | getattr(os, "O_NOFOLLOW", 0),
                              0o600)
        except OSError as error:
            self.fd = None
            raise ControlledError(f"cannot open the session lock {self.path}: {error}") from error
        try:
            if not stat.S_ISREG(os.fstat(self.fd).st_mode):
                raise ControlledError(f"session lock {self.path} is not a regular file")
            self._lock()
        except (OSError, ControlledError):
            os.close(self.fd)
            self.fd = None
            raise
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
            self.release()
            raise ControlledError(f"cannot prepare the session lock {self.path}: {error}") from error
        return self

    def _lock(self):
        # Locked before the marker is read or written: on Windows the byte-range
        # lock is mandatory, so a contender must fail here, with the right message.
        try:
            os.lseek(self.fd, 0, os.SEEK_SET)
            if os.name == "nt":
                msvcrt.locking(self.fd, msvcrt.LK_NBLCK, 1)
            else:
                fcntl.flock(self.fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except OSError as error:
            raise ControlledError(
                f"another comparison already holds the session lock {self.path}: {error}"
            ) from error

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
