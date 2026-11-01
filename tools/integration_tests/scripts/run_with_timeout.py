#!/usr/bin/env python3
"""POSIX fallback for GNU timeout, including termination of child processes."""
import os
import signal
import subprocess
import sys

try:
    process = subprocess.Popen(sys.argv[2:], start_new_session=True)
except OSError as error:
    print(error, file=sys.stderr)
    sys.exit(127)
try:
    status = process.wait(timeout=float(sys.argv[1]))
except subprocess.TimeoutExpired:
    os.killpg(process.pid, signal.SIGTERM)
    try:
        process.wait(timeout=2)
    except subprocess.TimeoutExpired:
        pass
    # A descendant may ignore TERM even after its parent has exited.
    try:
        os.killpg(process.pid, signal.SIGKILL)
    except ProcessLookupError:
        pass
    process.wait()
    sys.exit(124)
sys.exit(status if status >= 0 else 128 - status)
