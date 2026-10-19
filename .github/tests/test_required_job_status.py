# SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

"""Execute the workflows' actual final-status shell scripts with job-result fixtures."""

import re
import subprocess
import unittest
from pathlib import Path

WORKFLOWS = (
    (
        "build.yaml",
        "build-status",
        "should_build",
        ("build-u24-x64", "build-u24-arm", "build-u22-x64", "build-u22-arm"),
    ),
    ("codecov.yaml", "coverage_final_status", "should_scan", ("codecov",)),
)


def decision_script(filename, job):
    text = (Path(__file__).resolve().parents[1] / "workflows" / filename).read_text()
    block = re.search(
        rf"(?ms)^  {re.escape(job)}:\n(.*?)(?=^  [\w-]+:|\Z)", text
    ).group(1)
    lines = block.splitlines()
    for index, line in enumerate(lines):
        if line.strip() == "run: |":
            indent = len(line) - len(line.lstrip()) + 2
            script = []
            for following in lines[index + 1 :]:
                if (
                    following.strip()
                    and len(following) - len(following.lstrip()) < indent
                ):
                    break
                script.append(following[indent:])
            return "\n".join(script)
    raise AssertionError(f"No decision script in {filename}:{job}")


class RequiredJobStatusTest(unittest.TestCase):
    def check_result(self, workflow, filter_result, should_run, results, expected):
        filename, job, output, dependencies = workflow
        values = {
            "needs.path-filter.result": filter_result,
            f"needs.path-filter.outputs.{output}": should_run,
        }
        values.update(
            {
                f"needs.{name}.result": result
                for name, result in zip(dependencies, results)
            }
        )
        script = re.sub(
            r"\$\{\{\s*([^}]+?)\s*\}\}",
            lambda match: values[match.group(1)],
            decision_script(filename, job),
        )
        result = subprocess.run(
            ["bash", "-e", "-o", "pipefail", "-c", script],
            capture_output=True,
            text=True,
        )
        self.assertEqual(
            result.returncode == 0, expected, result.stdout + result.stderr
        )

    def test_required_jobs_must_all_succeed(self):
        for workflow in WORKFLOWS:
            count = len(workflow[3])
            for should_run, expected_result in (
                ("true", "success"),
                ("false", "skipped"),
            ):
                self.check_result(
                    workflow, "success", should_run, [expected_result] * count, True
                )
                for index in range(count):
                    for status in ("success", "failure", "cancelled", "skipped", ""):
                        if status == expected_result:
                            continue
                        with self.subTest(
                            workflow=workflow[0],
                            should_run=should_run,
                            index=index,
                            status=status,
                        ):
                            results = [expected_result] * count
                            results[index] = status
                            self.check_result(
                                workflow, "success", should_run, results, False
                            )

    def test_skips_require_a_successful_filter_and_explicit_no_code(self):
        for workflow in WORKFLOWS:
            for should_run in ("true", "false", ""):
                for filter_result in ("success", "failure", "cancelled", "skipped", ""):
                    with self.subTest(
                        workflow=workflow[0],
                        filter_result=filter_result,
                        should_run=should_run,
                    ):
                        self.check_result(
                            workflow,
                            filter_result,
                            should_run,
                            ["skipped"] * len(workflow[3]),
                            filter_result == "success" and should_run == "false",
                        )

    def test_failed_filter_or_missing_output_never_passes(self):
        for workflow in WORKFLOWS:
            for filter_result, should_run in (
                ("failure", "true"),
                ("cancelled", "true"),
                ("success", ""),
            ):
                with self.subTest(
                    workflow=workflow[0],
                    filter_result=filter_result,
                    should_run=should_run,
                ):
                    self.check_result(
                        workflow,
                        filter_result,
                        should_run,
                        ["success"] * len(workflow[3]),
                        False,
                    )


if __name__ == "__main__":
    unittest.main()
