#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Hanabi
#
# SPDX-License-Identifier: LGPL-3.0-or-later

"""Native subprocess regressions for the smoke-test command executor."""

import contextlib
import io
import subprocess
import sys
import unittest

from .executor import CommandExecutor


class CommandExecutorTest(unittest.TestCase):
    def test_success(self):
        result = CommandExecutor().run(
            [sys.executable, "-c", "print('ready')"], timeout=5
        )
        self.assertEqual(result.returncode, 0)
        self.assertEqual(result.stdout.strip(), "ready")

    def test_checked_failure_logs_stderr(self):
        errors = io.StringIO()
        with contextlib.redirect_stderr(errors):
            with self.assertRaises(subprocess.CalledProcessError) as raised:
                CommandExecutor(verbose=True).run(
                    [sys.executable, "-c", "import sys; print('diagnostic', file=sys.stderr); sys.exit(7)"],
                    timeout=5,
                )
        self.assertEqual(raised.exception.returncode, 7)
        self.assertIn("    diagnostic", errors.getvalue())

    def test_unchecked_failure_logs_stderr(self):
        errors = io.StringIO()
        with contextlib.redirect_stderr(errors):
            result = CommandExecutor(verbose=True).run(
                [sys.executable, "-c", "import sys; print('diagnostic', file=sys.stderr); sys.exit(7)"],
                check=False, timeout=5,
            )
        self.assertEqual(result.returncode, 7)
        self.assertIn("    diagnostic", errors.getvalue())

    def test_verbose_timeout_logs_partial_stderr(self):
        # TimeoutExpired.stderr can be bytes or text depending on the platform.
        errors = io.StringIO()
        with contextlib.redirect_stderr(errors):
            with self.assertRaises(RuntimeError) as raised:
                CommandExecutor(verbose=True).run(
                    [sys.executable, "-c", "import sys,time; sys.stderr.write('waiting for repository\\nretry pending'); sys.stderr.flush(); time.sleep(10)"],
                    check=False, timeout=1,
                )
        cause = raised.exception.__cause__
        self.assertIsInstance(cause, subprocess.TimeoutExpired)
        diagnostic = cause.stderr.decode() if isinstance(cause.stderr, bytes) else cause.stderr
        self.assertEqual(diagnostic, "waiting for repository\nretry pending")
        self.assertIn("timed out after 1s", errors.getvalue())
        self.assertIn("    waiting for repository\n    retry pending", errors.getvalue())

    def test_quiet_timeout_preserves_exception_without_logging(self):
        errors = io.StringIO()
        with contextlib.redirect_stderr(errors):
            with self.assertRaises(RuntimeError) as raised:
                CommandExecutor().run(
                    [sys.executable, "-c", "import sys,time; print('diagnostic',file=sys.stderr,flush=True); time.sleep(10)"],
                    timeout=1,
                )
        self.assertIsInstance(raised.exception.__cause__, subprocess.TimeoutExpired)
        diagnostic = raised.exception.__cause__.stderr
        if isinstance(diagnostic, bytes):
            diagnostic = diagnostic.decode()
        self.assertIn("diagnostic", diagnostic)
        self.assertEqual(errors.getvalue(), "")

    def test_verbose_timeout_without_capture(self):
        errors = io.StringIO()
        with contextlib.redirect_stderr(errors):
            with self.assertRaises(RuntimeError) as raised:
                CommandExecutor(verbose=True).run(
                    [sys.executable, "-c", "import time; time.sleep(10)"],
                    capture_output=False, timeout=1,
                )
        self.assertIsNone(raised.exception.__cause__.stderr)
        self.assertIn("timed out after 1s", errors.getvalue())


if __name__ == "__main__":
    unittest.main(verbosity=2)
