#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

"""命令执行器的轻量单元测试。"""

import os
import subprocess
import unittest
from unittest import mock

from .executor import CommandExecutor


class CommandExecutorSudoFlagsTest(unittest.TestCase):
    """验证 sudo 参数默认值与环境变量覆盖行为。"""

    def run_with_sudo_flags(self, sudo_flags: str | None) -> list[str]:
        environment = {} if sudo_flags is None else {"SUDO_FLAGS": sudo_flags}
        clear_environment = sudo_flags is None
        completed = subprocess.CompletedProcess([], 0, "", "")

        with mock.patch.dict(os.environ, environment, clear=clear_environment):
            with mock.patch("subprocess.run", return_value=completed) as run:
                CommandExecutor().run(["ll-cli", "list"], sudo=True)

        return run.call_args.args[0]

    def test_uses_askpass_by_default(self):
        self.assertEqual(
            self.run_with_sudo_flags(None),
            ["sudo", "-A", "ll-cli", "list"],
        )

    def test_splits_custom_flags_with_shell_quoting(self):
        self.assertEqual(
            self.run_with_sudo_flags('--preserve-env="HTTP PROXY" -n'),
            ["sudo", "--preserve-env=HTTP PROXY", "-n", "ll-cli", "list"],
        )

    def test_allows_empty_flags(self):
        self.assertEqual(
            self.run_with_sudo_flags(""),
            ["sudo", "ll-cli", "list"],
        )


if __name__ == "__main__":
    unittest.main()
