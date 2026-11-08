#!/usr/bin/env python3
"""Regression checks for shell test evidence, deadlines and file discovery.

Run: python3 tools/integration_tests/scripts/test_shell_helpers.py
Requires Python 3, Bash and DCMTK dcmdump. No PACS build or server is needed.
"""
from pathlib import Path
import ctypes
import os
import shutil
import signal
import socket
import subprocess
import sys
import tempfile
import time
import unittest

SCRIPTS = Path(__file__).resolve().parent
DATA = SCRIPTS.parent / "test_data"


class ShellHelpers(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="pacs-shell-helpers-")
        self.root = Path(self.temp.name)
        self.build = self.root / "build"
        (self.build / "bin").mkdir(parents=True)
        self.env = {**os.environ, "PACS_TEST_LOG_DIR": str(self.root / "logs")}

    def tearDown(self):
        self.temp.cleanup()

    def inventory(self, *args):
        return subprocess.run([sys.executable, str(SCRIPTS / "dicom_inventory.py"), *map(str, args)],
                              capture_output=True, text=True, timeout=15)

    def shell(self, body, *args):
        return subprocess.run(["bash", "-c", 'source "$1/binary_common.sh"; init_suite helper "$2" || exit 2; '
                               + body, "_", str(SCRIPTS), str(self.build), *map(str, args)],
                              env=self.env, capture_output=True, text=True, timeout=15)

    def test_extensionless_and_misleading_names(self):
        received = self.root / "received objects"
        received.mkdir()
        shutil.copyfile(DATA / "ct_minimal.dcm", received / "CT.extensionless")
        shutil.copyfile(DATA / "mr_minimal.dcm", received / "MR.1.2.3")
        (received / "not-dicom.dcm").write_text("not a DICOM object")
        (received / "storescp.log").write_text("received one file")
        (received / "truncated.dcm").write_bytes(bytes(128) + b"DICM")
        result = self.inventory("count", received)
        self.assertEqual((result.returncode, result.stdout.strip()), (0, "2"), result.stderr)
        result = self.inventory("verify", received, DATA / "ct_minimal.dcm", DATA / "mr_minimal.dcm")
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_exact_uid_comparison_rejects_wrong_missing_extra_and_duplicate(self):
        received = self.root / "received"
        received.mkdir()
        ct, mr = DATA / "ct_minimal.dcm", DATA / "mr_minimal.dcm"
        self.assertEqual(self.inventory("verify", received, ct).returncode, 1)
        shutil.copyfile(mr, received / "wrong")
        self.assertEqual(self.inventory("verify", received, ct).returncode, 1)
        shutil.copyfile(ct, received / "right")
        self.assertEqual(self.inventory("verify", received, ct).returncode, 1)
        (received / "wrong").unlink()
        self.assertEqual(self.inventory("verify", received, ct).returncode, 0)
        shutil.copyfile(ct, received / "duplicate")
        self.assertEqual(self.inventory("verify", received, ct).returncode, 1)

    def test_query_checks_every_response_and_expected_count(self):
        responses = self.root / "responses"
        responses.mkdir()
        uid = self.inventory("value", DATA / "ct_minimal.dcm", "0020,000d").stdout.strip()
        self.assertEqual(self.inventory("query", responses, "0020,000d").returncode, 0)
        self.assertEqual(self.inventory("query", responses, "0020,000d", uid).returncode, 1)
        shutil.copyfile(DATA / "ct_minimal.dcm", responses / "rsp")
        self.assertEqual(self.inventory("query", responses, "0020,000d", uid).returncode, 0)
        (responses / "broken-response").write_bytes(b"broken")
        self.assertEqual(self.inventory("query", responses, "0020,000d", uid).returncode, 1)

    def test_command_exit_and_output_are_preserved(self):
        result = self.shell("run_cli 2 bash -c 'echo diagnostic; exit 7'")
        self.assertEqual(result.returncode, 7, result.stdout + result.stderr)
        self.assertEqual(next((self.root / "logs").rglob("command-1.log.exit")).read_text(), "7\n")
        self.assertIn("diagnostic", next((self.root / "logs").rglob("command-1.log")).read_text())

    def test_empty_suite_fails(self):
        result = self.shell("finish_suite")
        self.assertEqual(result.returncode, 1)
        self.assertIn("Tests run: 0", result.stdout)

    def test_missing_binary_fails_prerequisite(self):
        result = self.shell("check_cli_contract missing 0 --port")
        self.assertEqual(result.returncode, 1)
        self.assertIn("not found", result.stdout)

    def test_startup_parse_failure_preserves_status_and_log(self):
        with socket.socket() as sock:
            sock.bind(("127.0.0.1", 0))
            port = sock.getsockname()[1]
        result = self.shell("start_server parse-error localhost \"$3\" bash -c 'echo invalid-arguments; exit 2'", port)
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn("exited during startup (2)", result.stdout)
        self.assertEqual(next((self.root / "logs").rglob("parse-error.log.exit")).read_text(), "2\n")

    def test_occupied_port_is_not_reused_or_killed(self):
        with socket.socket() as sock:
            sock.bind(("127.0.0.1", 0))
            sock.listen()
            port = sock.getsockname()[1]
            result = self.shell('start_server existing localhost "$3" bash -c "exit 0"', port)
            self.assertEqual(result.returncode, 1)
            self.assertIn("already in use", result.stdout)
            with socket.create_connection(("127.0.0.1", port), timeout=1):
                pass

    def assert_allocator_skips(self, port):
        result = self.shell('find_available_port "$3"', port)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        selected = [int(line) for line in result.stdout.splitlines() if line.isdigit()]
        self.assertEqual(len(selected), 1, result.stdout)
        self.assertGreater(selected[0], port)
        with socket.socket() as check:
            check.bind(('0.0.0.0', selected[0]))

    def test_allocator_skips_bound_port_before_listen(self):
        with socket.socket() as bound:
            bound.bind(('127.0.0.1', 0))
            self.assert_allocator_skips(bound.getsockname()[1])

    def test_allocator_skips_time_wait_after_listener_closes(self):
        with socket.socket() as listener:
            listener.bind(('127.0.0.1', 0))
            listener.listen()
            port = listener.getsockname()[1]
            with socket.create_connection(('127.0.0.1', port), timeout=1) as client:
                accepted, _ = listener.accept()
                # Server actively closes; the client acknowledges EOF before
                # closing so the server port enters TIME_WAIT deterministically.
                accepted.close()
                self.assertEqual(client.recv(1), b'')
        with socket.socket() as check:
            with self.assertRaises(OSError):
                check.bind(('0.0.0.0', port))
        self.assert_allocator_skips(port)

    def test_server_crash_during_cleanup_fails_suite(self):
        with socket.socket() as sock:
            sock.bind(("127.0.0.1", 0))
            port = sock.getsockname()[1]
        server = self.root / "server.py"
        server.write_text('import socket,sys,time\ns=socket.socket()\n'
                          's.bind(("127.0.0.1",int(sys.argv[1])))\ns.listen()\n'
                          'c,_=s.accept()\nc.close()\ntime.sleep(0.2)\nraise SystemExit(7)\n')
        result = self.shell('start_server crash localhost "$3" python3 "$4" "$3" || exit 1; sleep 1; true', port, server)
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn("exited unexpectedly (7)", result.stdout)

    def test_timeout_fallback_status_and_missing_program(self):
        fallback = [sys.executable, str(SCRIPTS / "run_with_timeout.py"), "1"]
        for status in [0, 7]:
            result = subprocess.run([*fallback, sys.executable, "-c", f"raise SystemExit({status})"], timeout=5)
            self.assertEqual(result.returncode, status)
        result = subprocess.run([*fallback, str(self.root / "missing")], capture_output=True, timeout=5)
        self.assertEqual(result.returncode, 127)

    def test_timeout_fallback_kills_term_ignoring_descendant(self):
        # Containers may have a PID 1 that never reaps orphans. Adopt the test
        # grandchild so this regression check can reap it after the deadline.
        libc = None
        if sys.platform.startswith("linux"):
            libc = ctypes.CDLL(None, use_errno=True)
            self.assertEqual(libc.prctl(36, 1, 0, 0, 0), 0)  # PR_SET_CHILD_SUBREAPER
        child_code = 'import signal,time; signal.signal(signal.SIGTERM, signal.SIG_IGN); print("ready", flush=True); time.sleep(60)'
        pid_file = self.root / "child.pid"
        parent_code = ('import subprocess,sys,pathlib,time; '
                       f'p=subprocess.Popen([sys.executable,"-c",{child_code!r}],stdout=subprocess.PIPE); '
                       'p.stdout.readline(); '
                       f'pathlib.Path({str(pid_file)!r}).write_text(str(p.pid)); '
                       'time.sleep(60)')
        start = time.monotonic()
        result = subprocess.run([sys.executable, str(SCRIPTS / "run_with_timeout.py"), "0.5",
                                 sys.executable, "-c", parent_code], timeout=8)
        self.assertEqual(result.returncode, 124)
        self.assertLess(time.monotonic() - start, 5)
        pid = int(pid_file.read_text())
        for _ in range(50):
            state = subprocess.run(["ps", "-o", "stat=", "-p", str(pid)], capture_output=True, text=True).stdout.strip()
            if not state or "Z" in state:
                break
            time.sleep(0.02)
        try:
            self.assertTrue(not state or "Z" in state, f"descendant still running: {pid} ({state})")
        finally:
            if state and "Z" not in state:
                os.kill(pid, signal.SIGKILL)
            if libc is not None:
                os.waitpid(pid, 0)
                self.assertEqual(libc.prctl(36, 0, 0, 0, 0), 0)

    def test_wrapper_uses_python_when_coreutils_absent(self):
        shim = self.root / "path"
        shim.mkdir()
        (shim / "python3").symlink_to(sys.executable)
        result = self.shell('PATH="$3" run_with_timeout 0.1 python3 -c "import time; time.sleep(10)"', shim)
        self.assertEqual(result.returncode, 124, result.stdout + result.stderr)


if __name__ == "__main__":
    if not shutil.which("dcmdump"):
        sys.exit("dcmdump is required; missing prerequisites must not produce a passing test run")
    unittest.main(verbosity=2)
