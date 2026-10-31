#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

"""CommandExecutor 的 sudo argv 回归测试。

运行方式（仓库根目录，无需 root）：
    python3 -m unittest tools.test_linglong.test_executor
"""

import os
import subprocess
import unittest
from unittest import mock

try:
    from . import executor as executor_module
except ImportError:  # 直接在本目录运行 python3 test_executor.py 时
    import executor as executor_module


def capture_run(**run_kwargs):
    """调用 CommandExecutor.run(["true"], sudo=True)，返回捕获的 argv。"""
    captured = {}

    def fake_run(argv, **kwargs):
        captured["argv"] = argv
        captured["env"] = kwargs.get("env")
        return subprocess.CompletedProcess(argv, 0, "", "")

    with mock.patch.object(executor_module.subprocess, "run", side_effect=fake_run):
        executor_module.CommandExecutor().run(["true"], sudo=True, **run_kwargs)
    return captured


class SudoFlagsTest(unittest.TestCase):
    def test_default_is_askpass(self):
        with mock.patch.dict(os.environ, {}, clear=True):
            self.assertEqual(capture_run()["argv"], ["sudo", "-A", "true"])

    def test_process_environment_value_is_used(self):
        with mock.patch.dict(os.environ, {"SUDO_FLAGS": "-n"}, clear=True):
            self.assertEqual(capture_run()["argv"], ["sudo", "-n", "true"])

    def test_quoted_group_stays_one_token(self):
        with mock.patch.dict(os.environ, {"SUDO_FLAGS": "-u 'dbus' producer"}, clear=True):
            self.assertEqual(
                capture_run()["argv"],
                ["sudo", "-u", "dbus", "producer", "true"],
            )

    def test_empty_value_disables_flags(self):
        with mock.patch.dict(os.environ, {"SUDO_FLAGS": ""}, clear=True):
            self.assertEqual(capture_run()["argv"], ["sudo", "true"])

    def test_env_argument_overrides_process_environment(self):
        with mock.patch.dict(os.environ, {"SUDO_FLAGS": "-A"}, clear=True):
            captured = capture_run(env={"SUDO_FLAGS": "-n"})
        self.assertEqual(captured["argv"], ["sudo", "-n", "true"])
        # env= 已合并进子进程环境，sudo 自身也能看到同一份配置
        self.assertEqual(captured["env"]["SUDO_FLAGS"], "-n")

    def test_env_argument_can_clear_default(self):
        with mock.patch.dict(os.environ, {"SUDO_FLAGS": "-n"}, clear=True):
            captured = capture_run(env={"SUDO_FLAGS": ""})
        self.assertEqual(captured["argv"], ["sudo", "true"])
        self.assertEqual(captured["env"]["SUDO_FLAGS"], "")

    def test_non_sudo_command_is_unchanged(self):
        captured = {}
        with mock.patch.dict(os.environ, {"SUDO_FLAGS": "-n"}, clear=True):
            with mock.patch.object(
                executor_module.subprocess,
                "run",
                side_effect=lambda argv, **kwargs: captured.update(argv=argv)
                or subprocess.CompletedProcess(argv, 0, "", ""),
            ):
                executor_module.CommandExecutor().run(["true"], sudo=False)
        self.assertEqual(captured["argv"], ["true"])

    def test_malformed_value_raises_clear_error(self):
        with mock.patch.dict(os.environ, {"SUDO_FLAGS": "-u 'unterminated"}, clear=True):
            with self.assertRaises(RuntimeError) as ctx:
                capture_run()
        self.assertIn("invalid SUDO_FLAGS", str(ctx.exception))


if __name__ == "__main__":
    unittest.main()
