#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

"""
玲珑冒烟测试 — 命令执行器

封装 subprocess 调用，提供统一的错误处理与日志输出。
"""

import os
import subprocess
import sys
from typing import Optional

SUDO_FLAGS = "-A"

class CommandExecutor:
    """命令执行器，封装 subprocess.run 的调用细节。"""

    def __init__(self, verbose: bool = False):
        self.verbose = verbose

    def run(
        self,
        cmd: list[str],
        *,
        sudo: bool = False,
        timeout: Optional[int] = None,
        check: bool = True,
        capture_output: bool = True,
        cwd: Optional[str] = None,
        env: Optional[dict] = None,
    ) -> subprocess.CompletedProcess:
        """
        执行命令并返回结果。

        参数:
            cmd: 命令列表
            sudo: 是否通过 sudo 执行
            timeout: 超时秒数
            check: 非零退出码时是否抛出异常
            capture_output: 是否捕获输出
            cwd: 工作目录
            env: 额外环境变量，会合并到当前环境

        返回:
            subprocess.CompletedProcess

        抛出:
            subprocess.CalledProcessError: 当 check=True 且命令失败时
            RuntimeError: 命令超时时
        """
        full_cmd = cmd[:]
        run_env = os.environ.copy()
        if env:
            run_env.update(env)
        if sudo:
            flags = SUDO_FLAGS
            full_cmd = ["sudo"] + (flags.split() if flags else []) + full_cmd

        try:
            result = subprocess.run(
                full_cmd,
                capture_output=capture_output,
                text=True,
                timeout=timeout,
                cwd=cwd,
                env=run_env,
                check=check,
            )
        except subprocess.CalledProcessError as e:
            # 仅当 check=True 且命令失败时进入此分支
            if self.verbose:
                cmd_str = " ".join(cmd)
                print(
                    f"  [ERROR] Command failed (exit={e.returncode}): {cmd_str}",
                    file=sys.stderr,
                )
                if e.stderr:
                    for line in e.stderr.strip().splitlines():
                        print(f"    {line}", file=sys.stderr)
            raise
        except subprocess.TimeoutExpired as e:
            cmd_str = " ".join(cmd)
            if self.verbose:
                print(
                    f"  [ERROR] Command timed out after {timeout}s: {cmd_str}",
                    file=sys.stderr,
                )
            raise RuntimeError(f"Command timed out after {timeout}s: {cmd_str}") from e
        else:
            # check=False 时，非零退出码也会进入此分支
            if not check and self.verbose and result.returncode != 0:
                cmd_str = " ".join(cmd)
                print(
                    f"  [ERROR] Command failed (exit={result.returncode}): {cmd_str}",
                    file=sys.stderr,
                )
                if result.stderr:
                    for line in result.stderr.strip().splitlines():
                        print(f"    {line}", file=sys.stderr)
            return result
