#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Linyaps contributors
#
# SPDX-License-Identifier: LGPL-3.0-or-later

import contextlib
from datetime import datetime, timezone
import io
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import Mock, patch

from tools.test_linglong import main, reporter


class SmallSmokeTest(main.SmokeTest):
    STEPS = [
        ("First step", "first_step", "test"),
        ("Second step", "second_step", "test"),
    ]

    def first_step(self):
        pass

    def second_step(self):
        pass


class SmokeTestRunTest(unittest.TestCase):
    def setUp(self):
        self.contexts = contextlib.ExitStack()
        self.addCleanup(self.contexts.close)
        # Keep the runner isolated from services, installed refs, and repositories.
        for name, value in (
            ("ensure_service_coverage_env", True),
            ("ensure_builder_cache_healthy", True),
            ("_snapshot_repo_config", "repositories: {}\n"),
            ("_installed_refs", set()),
        ):
            self.contexts.enter_context(
                patch.object(main.SmokeTest, name, return_value=value)
            )
        self.commands = self.contexts.enter_context(
            patch("subprocess.run", side_effect=AssertionError("unexpected subprocess"))
        )
        self.contexts.enter_context(contextlib.redirect_stdout(io.StringIO()))
        self.contexts.enter_context(contextlib.redirect_stderr(io.StringIO()))
        self.smoke_test = SmallSmokeTest()
        self.smoke_test.start_time = datetime(2026, 10, 17, tzinfo=timezone.utc)
        self.smoke_test.cleanup = Mock()

    def tearDown(self):
        self.commands.assert_not_called()

    def fail_first_step(self):
        self.smoke_test.first_step = Mock(side_effect=RuntimeError("step failed"))

    def fail_cleanup(self):
        self.smoke_test.cleanup.side_effect = RuntimeError("cleanup failed")

    def assert_statuses(self, statuses):
        self.assertEqual([r.status for r in self.smoke_test.results], statuses)

    def test_successful_cleanup_keeps_step_results(self):
        with patch.object(main, "generate_report") as generate_report:
            self.smoke_test.run()
        self.assertFalse(self.smoke_test.has_failed)
        self.assert_statuses(["PASS", "PASS"])
        self.smoke_test.cleanup.assert_called_once()
        generate_report.assert_called_once()
        self.assertIs(generate_report.call_args.kwargs["results"], self.smoke_test.results)

    def test_cleanup_failure_preserves_completed_steps(self):
        self.fail_cleanup()
        with patch.object(main, "generate_report") as generate_report:
            self.smoke_test.run()
        self.assertTrue(self.smoke_test.has_failed)
        self.assert_statuses(["PASS", "PASS", "FAIL"])
        failure = self.smoke_test.results[-1]
        self.assertEqual(failure.index, len(self.smoke_test.STEPS) + 1)
        self.assertEqual(failure.title, "清理测试环境")
        self.assertEqual(failure.category, "环境清理")
        self.assertEqual(failure.error_message, "cleanup failed")
        self.assertGreaterEqual(failure.duration_ms, 0)
        generate_report.assert_called_once()

    def test_failed_step_still_cleans_up_and_reports(self):
        self.fail_first_step()
        with patch.object(main, "generate_report") as generate_report:
            self.smoke_test.run()
        self.assertTrue(self.smoke_test.has_failed)
        self.assert_statuses(["FAIL", "SKIPPED"])
        self.assertEqual(self.smoke_test.results[0].error_message, "step failed")
        self.smoke_test.cleanup.assert_called_once()
        generate_report.assert_called_once()

    def test_step_and_cleanup_failures_are_both_retained(self):
        self.fail_first_step()
        self.fail_cleanup()
        with patch.object(main, "generate_report") as generate_report:
            self.smoke_test.run()
        self.assert_statuses(["FAIL", "SKIPPED", "FAIL"])
        self.assertEqual(self.smoke_test.results[0].error_message, "step failed")
        self.assertEqual(self.smoke_test.results[-1].error_message, "cleanup failed")
        generate_report.assert_called_once()

    def test_cleanup_failure_is_serialized_to_json(self):
        self.fail_cleanup()
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "results.json"
            with (
                patch.object(main, "RESULTS_FILE", str(path)),
                patch.object(reporter, "HAS_OPENPYXL", False),
            ):
                self.smoke_test.run()
            summary = json.loads(path.read_text(encoding="utf-8"))
        self.assertEqual(summary["total_steps"], 3)
        self.assertEqual(summary["passed"], 2)
        self.assertEqual(summary["failed"], 1)
        self.assertEqual(summary["steps"][-1]["error_message"], "cleanup failed")

    @unittest.skipUnless(reporter.HAS_OPENPYXL, "openpyxl is optional")
    def test_cleanup_failure_is_written_to_excel(self):
        from openpyxl import load_workbook

        self.fail_cleanup()
        with tempfile.TemporaryDirectory() as directory:
            previous = os.getcwd()
            try:
                os.chdir(directory)
                with patch.object(
                    main, "RESULTS_FILE", str(Path(directory) / "results.json")
                ):
                    self.smoke_test.run()
                workbook = load_workbook(
                    io.BytesIO(Path("玲珑冒烟测试.xlsx").read_bytes())
                )
                try:
                    values = list(workbook.active.values)
                finally:
                    workbook.close()
            finally:
                os.chdir(previous)
        self.assertEqual(values[-1][2], "清理测试环境")
        self.assertEqual(values[-1][3], "FAIL")
        self.assertEqual(values[-1][5], "cleanup failed")

    def assert_cli_exit(self, expected):
        with (
            patch.object(main, "SmokeTest", return_value=self.smoke_test),
            patch.object(main, "ensure_session_bus_env"),
            patch.object(main, "generate_report") as generate_report,
            patch("sys.argv", ["smoke-test"]),
            self.assertRaises(SystemExit) as raised,
        ):
            main.main()
        self.assertEqual(raised.exception.code, expected)
        generate_report.assert_called_once()

    def test_cli_returns_success_after_successful_cleanup(self):
        self.assert_cli_exit(0)

    def test_cli_returns_failure_after_cleanup_exception(self):
        self.fail_cleanup()
        self.assert_cli_exit(1)

    def test_reporter_error_is_not_silently_suppressed(self):
        self.fail_cleanup()
        with (
            patch.object(main, "generate_report", side_effect=OSError("report failed")),
            self.assertRaisesRegex(OSError, "report failed"),
        ):
            self.smoke_test.run()
        self.assertTrue(self.smoke_test.has_failed)
        self.assertEqual(self.smoke_test.results[-1].error_message, "cleanup failed")

    def test_step_keyboard_interrupt_propagates_after_cleanup_and_reporting(self):
        self.smoke_test.first_step = Mock(side_effect=KeyboardInterrupt)
        with (
            patch.object(main, "generate_report") as generate_report,
            self.assertRaises(KeyboardInterrupt),
        ):
            self.smoke_test.run()
        self.smoke_test.cleanup.assert_called_once()
        generate_report.assert_called_once()

    def test_cleanup_keyboard_interrupt_propagates_after_reporting(self):
        self.smoke_test.cleanup.side_effect = KeyboardInterrupt
        with (
            patch.object(main, "generate_report") as generate_report,
            self.assertRaises(KeyboardInterrupt),
        ):
            self.smoke_test.run()
        self.assert_statuses(["PASS", "PASS"])
        generate_report.assert_called_once()


if __name__ == "__main__":
    unittest.main()
