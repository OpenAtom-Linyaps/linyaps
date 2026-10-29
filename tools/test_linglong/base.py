#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

"""玲珑冒烟测试 — 基类：辅助设施与步骤执行框架。"""

import getpass
import json
import os
import pty
import shutil
import tempfile
import re
import select
import signal
import socket
import subprocess
import sys
import time
from datetime import datetime
from pathlib import Path

from .constants import (
    DEMO_APP_ID,
    DEMO_ARCH,
    DEMO_PROJECT_DIR,
    CALENDAR_APP_ID,
    SEMVER_APP_ID,
    TESTSUITE_BASELINE_APP_ID,
    NOTIF_BUS_DIR,
    NOTIF_BUS_SOCKET,
    NOTIF_BUS_PID,
    NOTIF_STUB_LOG,
    NOTIF_STUB_OUT,
    NOTIF_STUB_PID,
    NOTIF_INSTALL_TIMEOUT,
    DRIVER_DETECT_BINARY,
    LL_CLI,
    LL_BUILDER,
    REPO_CONFIG_PATH,
    RESULTS_FILE,
    JUNIT_FILE,
)
from .executor import SUDO_FLAGS, CommandExecutor
from .models import RepoState, StepResult, StepSkipped
from .reporter import generate_report, generate_junit_xml


class SmokeTestBase:



    def __init__(self, dated: bool = False, verbose: bool = False,
                 fail_fast: bool = False):
        self.results: list[StepResult] = []
        self.start_time = datetime.now().astimezone()
        self.dated = dated
        self.verbose = verbose
        self.fail_fast = fail_fast
        self.has_failed = False
        # 状态已不可用的步骤（方法名）。后面步骤的 needs 命中它时才会被跳过，
        # 而不是像原来那样"一步失败、后面全 SKIPPED"。
        self.unavailable: set = set()

        self.ll_cli_state = RepoState()
        self.ll_builder_state = RepoState()

        self.executor = CommandExecutor(verbose=verbose)

        # 校验步骤方法名：确保 STEPS 中每个方法都已定义
        for entry in self.STEPS:
            if not hasattr(self, entry[1]):
                raise AttributeError(
                    f"Step '{entry[0]}' references method '{entry[1]}' "
                    f"which is not defined on SmokeTest"
                )
        # 方法名 -> 步骤标题，用于报告"被哪个前置步骤挡住"
        self._titles = {entry[1]: entry[0] for entry in self.STEPS}

    # ── Command Execution Wrappers ──
    def _run_cmd(self, cmd: list[str], **kwargs) -> subprocess.CompletedProcess:
        return self.executor.run(cmd, **kwargs)

    def _sudo_ll_cli(self, *args: str, **kwargs) -> subprocess.CompletedProcess:
        return self._run_cmd([LL_CLI, *args], sudo=True, **kwargs)

    def _ll_cli(self, *args: str, **kwargs) -> subprocess.CompletedProcess:
        return self._run_cmd([LL_CLI, *args], **kwargs)

    def _ll_builder(self, *args: str, **kwargs) -> subprocess.CompletedProcess:
        return self._run_cmd([LL_BUILDER, *args], **kwargs)

    # ── Step Execution ──
    def _print_step_result(self, title: str, status: str):
        c = {"PASS": "\033[92m", "FAIL": "\033[31m", "SKIPPED": "\033[33m"}.get(
            status, ""
        )
        print(f"{c}[{status}]\033[0m {title}")


    # 整轮开始前的仓库配置快照，供 cleanup() 校验"改了要还原"。
    _repo_config_at_start = None
    _installed_refs_at_start = None

    # ── repo 配置快照 / 还原 ──
    @staticmethod
    def _normalize_repo_config(text: str) -> list:
        """把仓库配置归一化成一个"行的多重集合"，用来做还原校验。

        故意【不做逐字节比较】：服务/CLI 重写这个文件时，键的顺序、
        缩进、空行都可能变，但语义没变，逐字节比会误报。
        排序后逐行比：值的任何变化、仓库的增删都会露出来，
        而纯粹的排版差异不会。
        """
        return sorted(ln.strip() for ln in text.splitlines() if ln.strip())

    def _read_root_file(self, path) -> str:
        """读一个通常需要 root 的文件，返回【干净】的内容。

        ⚠️ 不要直接拿 `sudo -A cat` 的 stdout 当文件内容：sudo 在需要
        认证时会把 "验证成功" 这类 PAM 消息打到 stdout 上（时间戳还有
        效时又不打），于是同一份文件两次读出来会凭空多一行 —— 拿它做
        精确比较或回写就会出错。实测就是这么误报了一次"配置没还原"。

        这些文件实际是 644，普通用户就能读，所以先直接读；万一以后权限
        收紧，退回 `sudo cp` 到临时文件、再【不带 sudo】读临时文件，
        这样 sudo 的杂音只落在被丢弃的 cp stdout 上。
        """
        r = self._run_cmd(["cat", str(path)], check=False)
        if r.returncode == 0:
            return r.stdout
        tmpdir = Path(tempfile.mkdtemp(prefix="ll-rootfile-"))
        try:
            tmp = tmpdir / "content"
            r = self._run_cmd(["cp", str(path), str(tmp)], sudo=True,
                              check=False)
            if r.returncode != 0:
                raise RuntimeError(
                    f"读取 {path} 失败: rc={r.returncode} {r.stderr[:200]}")
            return tmp.read_text()
        finally:
            shutil.rmtree(tmpdir, ignore_errors=True)

    def _snapshot_repo_config(self) -> str:
        """读出仓库配置原文，供用完后还原。"""
        return self._read_root_file(REPO_CONFIG_PATH)

    def _restart_package_manager(self, timeout: int = 90) -> None:
        """重启 PackageManager 服务，并等它真的回到 active。

        ⚠️ systemctl restart 是异步的：返回时单元可能还在 activating，
        所以必须轮询等待，不能立刻判定。
        """
        svc = "org.deepin.linglong.PackageManager.service"
        if not shutil.which("systemctl"):
            return
        self._run_cmd(["systemctl", "reset-failed", svc], sudo=True,
                      check=False)
        self._run_cmd(["systemctl", "restart", svc], sudo=True, check=False,
                      timeout=300)
        deadline = time.time() + timeout
        state = ""
        while time.time() < deadline:
            r = self._run_cmd(["systemctl", "is-active", svc], check=False)
            state = r.stdout.strip()
            if state == "active":
                return
            time.sleep(2)
        raise RuntimeError(f"重启后 PackageManager 服务未回到 active: {state!r}")

    def _restore_repo_config(self, snapshot: str) -> None:
        """把仓库配置写回快照内容；已经一致就跳过。

        ⚠️ 不能靠 `repo enable-mirror` / `disable-mirror` 之类的命令拼回原状：
        disable-mirror 并不会清掉 region，优先级顺序也被改动过，
        用命令是拼不回来的。直接写回原文最忠实。

        ⚠️⚠️ 但【光写文件不够】：PackageManager 服务把仓库配置缓存在内存里，
        之后任何走服务的配置写入（repo add / remove / set-priority …）都会
        按它内存里的旧状态重写整个文件，把这里的还原直接冲掉。
        实测（2026-09，冒烟机）：
            repo disable-mirror stable   -> 文件 mirror_enabled: false
            把文件改回 true（模拟还原）
            repo add <临时仓库>           -> 文件又变回 false
        这就是"整轮跑完 stable 的 mirror_enabled 被关掉"的真正原因 ——
        之前只在用例前后写回文件，看着还原了，后面随便一条服务端写命令
        又把它冲掉了。
        所以写完文件必须让服务重新加载（重启），否则还原等于没做。
        """
        if self._snapshot_repo_config() == snapshot:
            return
        tmpdir = Path(tempfile.mkdtemp(prefix="ll-repocfg-"))
        try:
            tmp = tmpdir / "config.yaml"
            tmp.write_text(snapshot)
            # 目标文件已存在，cp 是就地截断写入，属主/权限保持不变
            r = self._run_cmd(["cp", str(tmp), str(REPO_CONFIG_PATH)],
                              sudo=True, check=False)
            if r.returncode != 0:
                raise RuntimeError(
                    f"写回 {REPO_CONFIG_PATH} 失败: rc={r.returncode} "
                    f"{r.stderr[:200]}")
        finally:
            shutil.rmtree(tmpdir, ignore_errors=True)
        if self._snapshot_repo_config() != snapshot:
            raise RuntimeError(f"写回后 {REPO_CONFIG_PATH} 与快照仍不一致")
        # 让服务丢掉内存里的旧配置，重新从文件加载
        self._restart_package_manager()
        if self._snapshot_repo_config() != snapshot:
            raise RuntimeError(
                f"重启服务后 {REPO_CONFIG_PATH} 又和快照不一致了"
                "（说明还有别的进程在按旧配置回写）")

    def _verify_repo_config_restored(self) -> bool:
        """整轮结束后，仓库配置必须和开始前语义一致。

        为什么单靠 _verify_repo_state_restored 不够：它只看默认仓库和
        最高优先级仓库，镜像开关 / region / 仓库地址被改是查不出来的。
        实测就漏过 —— test_repo_mirror_management 收尾停在
        disable-mirror stable，把机器上原本 mirror_enabled: true 的
        stable 关掉了，而校验一路绿灯。
        """
        if self._repo_config_at_start is None:
            return True
        try:
            now = self._snapshot_repo_config()
        except Exception as exc:  # noqa: BLE001
            print(f"  Error: 读取仓库配置失败，无法校验还原: {exc}",
                  file=sys.stderr)
            return False
        before = self._normalize_repo_config(self._repo_config_at_start)
        after = self._normalize_repo_config(now)
        if before == after:
            return True
        print(f"  Error: 冒烟结束后 {REPO_CONFIG_PATH} 与开始前不一致",
              file=sys.stderr)
        for ln in sorted(set(before) - set(after)):
            print(f"    丢失: {ln!r}", file=sys.stderr)
        for ln in sorted(set(after) - set(before)):
            print(f"    多出/被改: {ln!r}", file=sys.stderr)
        return False

    @staticmethod
    def _builder_ref(app_id: str, version: str = "1.0.0.1",
                     module: str | None = None) -> str:
        """拼出 `ll-builder remove` 需要的【完整引用】。

        ⚠️ 只给包名是不行的：Reference::parse 的正则要求
        channel:id/version/arch 四段（reference.cpp:27），对不上就直接
        返回 "regexp mismatched."；而 cmdRemoveApp 对每个 ref 只是打印
        一行错误就 continue，最后仍返回成功 —— 于是【静默什么都不删】。

        后果不只是测试没测到：探针包会一直留在构建仓库里，
        states.json 的 layer 记录越积越多，最后把 mergeModules 拖垮，
        表现为后面随机报 "stage pull dependency error"。
        """
        ref = f"main:{app_id}/{version}/{DEMO_ARCH}"
        if module:
            ref = f"{ref}/{module}"
        return ref

    # ── Step Execution ──
    def _step_label(self, method_name: str) -> str:
        return self._titles.get(method_name, method_name)

    def _light_cleanup(self):
        """步骤失败后轻量收尾：杀掉残留容器，避免脏状态连累后续步骤。

        _cleanup_containers 在 ps 为空时立即返回，只花一次
        `ll-cli ps --json`，所以每次失败都做一次是划算的。
        """
        for app in (DEMO_APP_ID, CALENDAR_APP_ID):
            try:
                self._cleanup_containers(app, timeout=20)
            except Exception:  # noqa: BLE001
                pass

    def run_step(self, title: str, step_index: int):
        entry = self.STEPS[step_index]
        method_name = entry[1]
        category = entry[2] if len(entry) > 2 else ""
        needs = entry[3] if len(entry) > 3 else ()
        method = getattr(self, method_name, None)
        if method is None:
            raise AttributeError(f"Test method not found: {method_name}")

        # 只跳过真正依赖失败步骤的那些。以前是一刀切 fail-fast：
        # 任何一步失败，后面全部 SKIPPED，一个用例的问题会让后面几十步的
        # 覆盖全部丢失（实测 71% 跌到 45%）。
        blocked = [m for m in needs if m in self.unavailable]
        if blocked:
            reason = "前置步骤失败或跳过: " + ", ".join(
                self._step_label(m) for m in blocked)
            self.results.append(
                StepResult(
                    index=step_index + 1,
                    title=title,
                    status="SKIPPED",
                    duration_ms=0,
                    error_message=reason,
                    category=category,
                )
            )
            print(f"  跳过原因: {reason}", file=sys.stderr)
            self._print_step_result(title, "SKIPPED")
            self.unavailable.add(method_name)
            return

        start = time.time_ns()
        print(f"\n==> {title}")
        restore_error = None
        try:
            # 会改机器仓库配置的步骤：执行前后自动快照 / 还原
            snapshot = None
            if method_name in self.REPO_CONFIG_STEPS:
                snapshot = self._snapshot_repo_config()
            try:
                method()
            finally:
                if snapshot is not None:
                    try:
                        self._restore_repo_config(snapshot)
                    except Exception as exc:  # noqa: BLE001
                        # 这里正在 finally 里，直接抛会盖掉用例本身的异常，
                        # 所以先记下来，等用例没抛错时再据此判 FAIL。
                        restore_error = exc
                        print(f"  Error: 还原仓库配置失败: {exc}",
                              file=sys.stderr)
            if restore_error is not None:
                raise AssertionError(
                    f"用例改了 {REPO_CONFIG_PATH} 但没有还原成功: "
                    f"{restore_error}")
            elapsed = (time.time_ns() - start) // 1_000_000
            self.results.append(
                StepResult(
                    index=step_index + 1,
                    title=title,
                    status="PASS",
                    duration_ms=elapsed,
                    category=category,
                )
            )
            self._print_step_result(title, "PASS")
        except Exception as e:  # noqa: BLE001
            # 前置条件不满足（比如仓库里的测试包被清理掉了）：记 SKIPPED，
            # 但【不】fail-fast。否则一个测试包消失就会让后面几十个用例
            # 全变成 SKIPPED，把真正的问题淹没。
            # ⚠️ 只有确实不是被测代码的问题才允许走这条路（见 StepSkipped）。
            elapsed = (time.time_ns() - start) // 1_000_000
            if isinstance(e, StepSkipped) and restore_error is None:
                self.results.append(
                    StepResult(
                        index=step_index + 1,
                        title=title,
                        status="SKIPPED",
                        duration_ms=elapsed,
                        error_message=str(e),
                        category=category,
                    )
                )
                print(f"  跳过原因: {e}", file=sys.stderr)
                self._print_step_result(title, "SKIPPED")
                self.unavailable.add(method_name)
                return
            self.results.append(
                StepResult(
                    index=step_index + 1,
                    title=title,
                    status="FAIL",
                    duration_ms=elapsed,
                    error_message=str(e),
                    category=category,
                )
            )
            print(f"  Error: {e}", file=sys.stderr)
            if restore_error is not None and str(restore_error) not in str(e):
                print(f"  Error: 另：还原仓库配置也失败: {restore_error}",
                      file=sys.stderr)
            self._print_step_result(title, "FAIL")
            self.has_failed = True
            self.unavailable.add(method_name)
            self._light_cleanup()
            if self.fail_fast:
                # 需要复现"第一个失败点"时用：后面的步骤全部记 SKIPPED。
                for i in range(step_index + 1, len(self.STEPS)):
                    self.results.append(
                        StepResult(
                            index=i + 1,
                            title=self.STEPS[i][0],
                            status="SKIPPED",
                            duration_ms=0,
                            error_message="fail-fast: 前序步骤失败",
                            category=(self.STEPS[i][2]
                                      if len(self.STEPS[i]) > 2 else ""),
                        )
                    )
                raise


    def run(self):
        # 先给 PackageManager 服务装上覆盖率环境：
        # 服务不继承脚本的环境变量，不这样做服务端覆盖率会写去编译期
        # 路径而被完全忽略（安装/卸载/entries 导出都在服务里）。
        # 失败不阻断冒烟本身，只影响覆盖率收集，所以只告警。
        try:
            if not self.ensure_service_coverage_env():
                print("Warning: 未能为 PackageManager 服务设置覆盖率环境，"
                      "服务端覆盖率可能缺失")
        except Exception as exc:  # noqa: BLE001
            print(f"Warning: 设置服务覆盖率环境失败: {exc}")

        # 记下仓库配置的原样，cleanup() 结束时校验它没被改坏。
        # 冒烟会加一个临时仓库 smoketesting，cleanup 里的
        # reset_repositories() 会把它删掉，所以首尾应当一致。
        try:
            self._repo_config_at_start = self._snapshot_repo_config()
            self._installed_refs_at_start = self._installed_refs()
        except Exception as exc:  # noqa: BLE001
            print(f"Warning: 读取仓库配置失败，跳过还原校验: {exc}")

        try:
            for i in range(len(self.STEPS)):
                title = self.STEPS[i][0]
                # 每步之前修一下构建缓存的自洽性（自愈兜底）。
                # 账本里若有 commit 已不存在的 layer 记录，之后任何一次
                # ll-builder build 的 mergeModules() 都会失败
                # （报 "stage pull dependency error"，重试无用）。
                # 注：这个不自洽在当前代码上没能复现（见
                # ensure_builder_cache_healthy 的说明），保留纯属兜底。
                # 这一步只是读一个 JSON + 查几个文件是否存在，很便宜。
                try:
                    self.ensure_builder_cache_healthy()
                except Exception as exc:  # noqa: BLE001
                    print(f"Warning: 检查构建缓存失败: {exc}")
                self.run_step(title, i)
        except Exception:
            pass
        finally:
            self.cleanup()
            generate_report(
                results=self.results,
                start_time=self.start_time,
                results_file=RESULTS_FILE,
                dated=self.dated,
            )
            try:
                generate_junit_xml(self.results, self.start_time, JUNIT_FILE)
            except Exception as exc:  # noqa: BLE001
                print(f"Warning: 生成 JUnit 报告失败: {exc}")


    def _parse_repo_show(self, cmd_type: str) -> tuple:
        if cmd_type == "ll-cli":
            result = self._ll_cli("repo", "show")
        else:
            result = self._ll_builder("repo", "show")
        output = result.stdout
        output = re.sub(r"\033\[[0-9;]*m", "", output)
        default_repo = ""
        highest_priority_repo = ""
        highest_priority = 0
        for line in output.splitlines():
            line = line.strip()
            if line.startswith("Default:"):
                parts = line.split()
                if len(parts) > 1:
                    default_repo = parts[1]
            parts = line.split()
            if len(parts) >= 2 and parts[-1].lstrip("-").isdigit():
                highest_priority_repo = parts[-2]
                highest_priority = int(parts[-1])
                break
        if not default_repo or not highest_priority_repo:
            raise RuntimeError(f"Failed to parse repo show: {output}")
        return default_repo, highest_priority_repo, highest_priority

    # ── Test: 配置冒烟测试仓库 ──
    def _next_repo_priority(self, current: int) -> int:
        return current + 100

    # ── Test: 创建 demo 项目 ──
    def _remove_demo_project_dir(self):
        demo_dir = Path(DEMO_PROJECT_DIR)
        if not demo_dir.exists():
            return
        abs_dir = demo_dir.resolve()
        if str(abs_dir) == "/" or (
            abs_dir.is_absolute()
            and Path.cwd() not in abs_dir.parents
            and abs_dir != Path.cwd()
        ):
            print(f"  [SECURITY] Refusing to remove: {abs_dir}", file=sys.stderr)
            return
        self._run_cmd(["rm", "-rf", str(demo_dir)], sudo=True, check=False)

    # ── Calendar Module Helpers ──
    def _list_calendar_modules(self) -> str:
        result = self._ll_cli("list")
        return "\n".join(l for l in result.stdout.splitlines() if CALENDAR_APP_ID in l)

    def _assert_calendar_module_present(self, module: str):
        modules = self._list_calendar_modules()
        if module not in modules:
            raise AssertionError(
                f"Missing module {CALENDAR_APP_ID}: {module}\nCurrent:\n{modules}"
            )

    def _assert_calendar_module_absent(self, module: str):
        modules = self._list_calendar_modules()
        if module in modules:
            raise AssertionError(
                f"Unexpected module {CALENDAR_APP_ID}: {module}\nCurrent:\n{modules}"
            )

    def _assert_calendar_modules_after_downgrade(self):
        self._assert_calendar_module_present("binary")
        self._assert_calendar_module_present("develop")
        self._assert_calendar_module_present("lang-ja")
        self._assert_calendar_module_absent("unuse")

    def _assert_calendar_modules_after_upgrade(self):
        self._assert_calendar_module_present("binary")
        self._assert_calendar_module_present("develop")
        self._assert_calendar_module_absent("lang-ja")

    # ── Cleanup ──
    def _verify_repo_state_restored(self, cmd_type: str) -> bool:
        if cmd_type == "ll-cli":
            expected = self.ll_cli_state
        else:
            expected = self.ll_builder_state

        if not expected.default_repo or not expected.highest_priority_repo:
            return True  # 初始状态未记录，跳过检查

        try:
            actual_default, actual_hr, actual_hp = self._parse_repo_show(cmd_type)
        except RuntimeError:
            return False

        ok = True
        if actual_default != expected.default_repo:
            print(
                f"{cmd_type} default repo changed: "
                f"{expected.default_repo} -> {actual_default}",
                file=sys.stderr,
            )
            ok = False
        if actual_hr != expected.highest_priority_repo:
            print(
                f"{cmd_type} highest priority repo changed: "
                f"{expected.highest_priority_repo} -> {actual_hr}",
                file=sys.stderr,
            )
            ok = False
        if actual_hp != expected.highest_priority:
            print(
                f"{cmd_type} highest priority changed: "
                f"{expected.highest_priority} -> {actual_hp}",
                file=sys.stderr,
            )
            ok = False
        return ok

    # ── Helper: 在伪终端里执行命令 ──
    #
    # ll-cli enter 最终会调用 ll-box exec --tty，必须有真实 TTY，
    # 否则返回 65280（run command failed）。subprocess 默认没有 TTY，
    # 所以这里用 pty.fork() 造一个。
    @staticmethod
    def _run_in_pty(cmd: list, timeout: int = 30):
        pid, fd = pty.fork()
        if pid == 0:
            # 子进程：fd 就是它的 stdin/stdout/stderr
            try:
                os.execvp(cmd[0], cmd)
            finally:
                os._exit(127)
        buf = b""
        deadline = time.time() + timeout
        try:
            while time.time() < deadline:
                try:
                    chunk = os.read(fd, 4096)
                except OSError:
                    break
                if not chunk:
                    break
                buf += chunk
        finally:
            try:
                os.close(fd)
            except OSError:
                pass
        _, status = os.waitpid(pid, 0)
        return status >> 8, buf.decode("utf-8", "replace")

    # ── Helper: 等待容器出现并返回容器 ID ──
    def _wait_container(self, timeout: int = 40, app_id: str = "") -> str:
        """等 ps 里出现指定应用的容器，返回容器 ID（超时返回空串）。

        ⚠️ 不能直接取 ps 的第一条：机器上可能残留别的容器（GUI 应用被
        DBus 服务拉起、上一轮没退干净的），第一条不一定是我们刚起的。
        实测 run64/65：日历容器排在 demo 容器前面，导致容器生命周期用例
        把日历的容器当成 demo 的，误判失败。
        """
        deadline = time.time() + timeout
        while time.time() < deadline:
            r = self._ll_cli("ps", "--json", check=False)
            if r.returncode == 0:
                try:
                    arr = json.loads(r.stdout)
                except json.JSONDecodeError:
                    arr = []
                for item in arr:
                    pkg = item.get("package", "")
                    if not app_id or f"main:{app_id}/" in pkg:
                        return item.get("id", "")
            time.sleep(2)
        return ""

    # ── Helper: 彻底清理某个应用的容器 ──
    #
    # ll-cli kill 只是让容器退出，ll-box 进程可能还没回收。
    # 残留的 ll-box 会占住应用引用，使 sudo ll-cli uninstall 恒返回 255
    # （而且不打印任何错误），从而让后续所有安装步骤级联失败。
    # 这里反复 kill 直到 ps 里确实看不到该应用。
    def _cleanup_containers(self, app_id: str, timeout: int = 60):
        deadline = time.time() + timeout
        while time.time() < deadline:
            r = self._ll_cli("ps", "--json", check=False)
            try:
                arr = json.loads(r.stdout) if r.returncode == 0 else []
            except json.JSONDecodeError:
                arr = []
            if not arr:
                return
            for item in arr:
                cid = item.get("id")
                if cid:
                    self._ll_cli("kill", cid, timeout=60, check=False)
            self._ll_cli("kill", app_id, timeout=60, check=False)
            time.sleep(3)
        # 超时也返回，让上层的断言去报告问题，不在这里静默吞掉

    # ── Helper: 确保 ps 里没有容器（含服务端残留记录）──
    #
    # _cleanup_containers 只能杀掉还活着的容器；容器进程被 SIGKILL 后，
    # PackageManager 服务端可能留下"僵尸记录"，ps 仍会把它列出来，而对
    # 它执行 kill 只会得到 "no container found"。这时只能重启服务让它
    # 重新扫描容器目录。ps 用例要断言"无容器的中文提示"，必须先过这关。
    def _ensure_no_containers(self, app_id: str = "", timeout: int = 20):
        def _listed():
            r = self._ll_cli("ps", "--json", check=False)
            try:
                return json.loads(r.stdout) if r.returncode == 0 else []
            except json.JSONDecodeError:
                return []

        if not app_id:
            app_id = CALENDAR_APP_ID  # 真机实测最容易残留的是 GUI 日历应用
        self._cleanup_containers(app_id, timeout=timeout)
        if not _listed():
            return
        # 记录残留：重启服务重新扫描后再清一遍
        self._restart_package_manager()
        self._cleanup_containers(app_id, timeout=timeout)
        items = _listed()
        if items:
            ids = ", ".join(str(i.get("id", "?")) for i in items)
            raise AssertionError(f"清理后仍残留容器记录: {ids}")

    # ── Helper: 找一个可以直接运行的目标 ──
    #
    # 本批用例刻意不依赖 demo 构建产物（构建慢、且会被前面步骤删掉项目目录），
    # 改用已安装的 base 作为运行目标。base 一定存在（所有应用都依赖它），
    # 且 `ll-cli run <base> -- /bin/true` 实测 rc=0。
    def _find_runnable_target(self) -> str:
        # 本批用例排在流水线末尾，而前面的「清理未使用运行时（prune）」
        # 会把无人依赖的 base 一并移除，所以这里不能假设 base 一定还在。
        # 先看已装的，缺了就补装一个 —— 幂等，且 base 是所有应用的前提。
        r = self._ll_cli("list", "--type=base", check=False)
        if r.returncode == 0 and "org.deepin.base" in r.stdout:
            return "org.deepin.base"

        self._sudo_ll_cli("install", "org.deepin.base", timeout=600, check=False)

        r = self._ll_cli("list", "--type=base", check=False)
        if r.returncode == 0 and "org.deepin.base" in r.stdout:
            return "org.deepin.base"

        # 退回到 demo（若前面的步骤留下了它）
        listing = self._ll_cli("list", check=False)
        if DEMO_APP_ID in listing.stdout:
            return DEMO_APP_ID

        raise AssertionError(
            "既没有可用的 base，也补装失败，无法执行本用例")

    @staticmethod
    def _yaml_repo_block(cfg_text: str, alias: str) -> str:
        """从 config.yaml 里取出某个仓库那一段（按 `  - ` 分段，不依赖 YAML 库）。

        config.yaml 是缩进式的列表，每个仓库以 `  - ` 起头、到下一个
        `  - ` 或文件结束为止。取到这一段再在里面找字段，就不会把别的
        仓库的 mirror_enabled 误判成本仓库的。
        """
        blocks: list[list[str]] = []
        current: list[str] = []
        for line in cfg_text.splitlines():
            if line.startswith("  - "):
                if current:
                    blocks.append(current)
                current = [line]
            elif current:
                current.append(line)
        if current:
            blocks.append(current)

        for block in blocks:
            joined = "\n".join(block)
            if f"alias: {alias}" in joined or f"name: {alias}" in joined:
                return joined
        return ""

    # ── Helper: 在伪终端里跑 ll-cli 并回收输出 ──
    #
    # 为什么需要真 PTY：ll-cli 用
    #     isatty(STDIN_FILENO) && isatty(STDOUT_FILENO)      (cli.cpp:183)
    # 决定是否走 TTY 分支（分配 console PTY、经 unix socket 传 fd、
    # 传 winsize、raw mode、interaction 提示等）。subprocess 默认给的
    # 是管道，这些分支全部走不到 —— 实测 cli.cpp 长期只有 59%，
    # 换成 PTY 后到 72%，common/socket.cpp 从 0% 到 44%。
    #
    # 与 _run_in_pty 的区别：这里用非阻塞读 + 硬超时 + 超时强杀，
    # 因为 `ll-cli run` 有可能一直等不到 EOF（应用不退出），
    # 阻塞式 read 会把整个冒烟挂死。
    # ── 安装状态快照 / 完整还原 ──
    #
    # ⚠️ 血的教训（整轮 run45 暴露）：镜像里同一个应用的"最新版"可能和
    # 机器上装着的不一样 —— 日历实测 6.5.42.1 被换成了 5.14.5.1，重装
    # 还会顺带拉来旧 base 23.1.0.3。所以"卸载再装回来"会【静默换版本】，
    # 凡是动过安装状态的用例，都必须按快照里的【确切 id/版本/模块】还原。
    # ⚠️ 名称列可能是多个词（org.deepin.runtime.dtk 的名称是 "deepin
    # runtime"），所以名称不能写成 \S+ —— 那样整行会被静默丢弃，而被漏掉
    # 的 ref 又会被还原逻辑当成"多余的"卸掉（只漏 1 个时还不会触发
    # "一次卸载超过 2 个 ref" 的护栏，会真的被卸掉）。用 .*? 吃掉名称空格。

    def _installed_refs(self):
        out = self._ll_cli("list", check=False).stdout or ""
        return {f"{i}/{v}/{m}" for i, v, c, m in self._REF_ROW.findall(out)}

    def _restore_installed_refs(self, snapshot):
        """把安装状态还原成 snapshot（多退少补，带确切版本）。

        ⚠️ 必须能接受 2 段（id/版本）和 3 段（id/版本/模块）两种快照：
        整轮 run46 就是因为我传了 2 段快照进来，导致集合相减把【所有】
        应用都算成"多余的"并全部卸载，把机器清空了。
        """
        # ⚠️ 2 段快照（id/版本）不区分模块，【不能】当成"只有 binary" ——
        # 否则"多退"会把同 id/版本下 develop 等模块当成多余的卸掉
        # （run48 实测：base 的 develop 模块就是这样被删掉的）。
        # 做法：用当前实际存在的模块把 2 段条目补全。
        now = self._installed_refs()
        expanded = set()
        for r in snapshot:
            # ⚠️ 段数要按【斜杠数】判断：id/版本 是 1 个斜杠（ID 里是点，
            # 不是斜杠），id/版本/模块 才是 2 个。这里原来写成 == 2，于是
            # 2 段条目被当成 3 段、不做展开，永远匹配不上真实的
            # id/版本/模块 —— 结果"每条都算缺、每个真实 ref 都算多"，
            # 这就是历史上两次把机器清空的真正原因（护栏只是在打补丁）。
            if r.count("/") == 1:
                mods = {x for x in now if x.startswith(r + "/")}
                expanded |= mods or {f"{r}/binary"}
            else:
                expanded.add(r)
        snapshot = expanded
        if not snapshot:
            raise AssertionError(
                "拒绝用空快照还原安装状态（那会把机器上的应用全卸掉）")
        # 硬护栏：唯一绝不允许的情况是把机器上【当前所有】应用都卸掉
        # （那等于清空机器）。历史上这里是按"卸载数 >= 快照总数"的比例
        # 判据，本意是防快照格式不匹配导致的灾难，但会误伤合法场景：
        # 日历模块用例会合法地多出 7 个 ref（5.14.x 的 binary/develop/
        # lang-ja + 被顺带拉进来的旧 base/runtime），正好等于快照总数，
        # 于是还原被拒绝、机器被留在脏状态（实测 run64/run65 两次）。
        # 真正危险的只有"快照里一个应用都对不上"的情况。
        extra = now - snapshot
        if extra and extra >= now:
            raise AssertionError(
                f"拒绝：快照里没有任何已装应用，要卸载全部 {len(now)} 个 ref"
                f"（疑似快照不匹配，会清空机器）: {sorted(extra)}")
        for ref in sorted(extra):
            parts = ref.split("/")
            i, v = parts[0], parts[1]
            m = parts[2] if len(parts) > 2 else "binary"
            argv = ["uninstall", "--force", f"{i}/{v}"]
            if m and m != "binary":
                argv += ["--module", m]
            self._sudo_ll_cli(*argv, timeout=300, check=False)
        for ref in sorted(snapshot - now):
            parts = ref.split("/")
            i, v = parts[0], parts[1]
            m = parts[2] if len(parts) > 2 else "binary"
            argv = ["install", f"{i}/{v}"]
            if m and m != "binary":
                argv += ["--module", m]
            self._sudo_ll_cli(*argv, timeout=900, check=False)
        after = self._installed_refs()
        if after != snapshot:
            raise AssertionError(
                f"安装状态没还原干净: {sorted(snapshot)} -> {sorted(after)}")

    def _pty_ll_cli(self, *args: str, timeout: int = 60, env: dict | None = None,
                    stop_when: str | None = None, grace: int = 10,
                    input_text: str | None = None, sudo: bool = False):
        """在真 PTY 里跑 ll-cli，返回 (退出码, 去噪输出)。

        stop_when: 输出里出现该子串就【主动收尾】，不再傻等。
            有些命令按设计就不会自己退出（例如 `run --debug` 会起
            gdbserver 等调试器附加），死等超时会把冒烟拖到分钟级。
            覆盖数据在代码执行到时就已经产生，只要让进程正常退出即可。

        env: 额外环境变量。值为 None 表示【真正删掉】该变量
            （getenv 返回 nullptr 与返回 "" 在某些分支上并不等价）。

        input_text: 预先把这些字节写进 PTY。用于回答交互式提问
            （TerminalNotifier 会先打印 "your choice:" 再读 stdin）。
            不需要等提示出现再写 —— PTY 的行规程会把输入缓存住，
            等 std::cin 真的去读时自然就拿到了。

        sudo: 用 sudo 提权跑。install/uninstall 这类命令要过 polkit 授权，
            不提权会直接得到 "Error 9: not authorized"，根本走不到交互那一步。
            用 sudo -A + SUDO_ASKPASS：askpass 只是把密码 echo 出去、
            不去读终端，所以 ll-cli 的 stdin/stdout 仍然是这个 PTY，
            isatty() 成立，TTY 分支照常走。

        收尾用 SIGINT 而不是 SIGKILL：libgcov 在正常退出或 SIGINT 时
        才把计数写回 .gcda，SIGKILL 会丢掉这个进程的整份计数。
        """
        cmd = [LL_CLI, *args]
        if sudo:
            cmd = ["sudo", *SUDO_FLAGS.split(), *cmd]
        pid, fd = pty.fork()
        if pid == 0:
            run_env = os.environ.copy()
            # 非 sudo 的 ll-cli 走当前用户的 prefix（见 executor.py 的说明）；
            # 提权后进程身份是 root，必须换成 root 的 prefix，
            # 否则两边争抢同一份 .gcda（权限不对时 libgcov 直接丢掉整份计数）。
            if sudo:
                if run_env.get("GCOV_PREFIX_ROOT"):
                    run_env["GCOV_PREFIX"] = run_env["GCOV_PREFIX_ROOT"]
            else:
                run_env.setdefault("GCOV_PREFIX", "/var/tmp/linglong-cov-user")
            run_env["GCOV_PREFIX_STRIP"] = "0"
            run_env.setdefault("DISPLAY", ":0")
            if env:
                for k, v in env.items():
                    if v is None:
                        run_env.pop(k, None)
                    else:
                        run_env[k] = v
            try:
                os.execvpe(cmd[0], cmd, run_env)
            finally:
                os._exit(127)

        buf = b""
        if input_text:
            # 必须在切非阻塞之前写：几字节的输入不会填满 PTY 缓冲区，
            # 不会真的阻塞，但省掉一次 partial-write 的处理。
            try:
                os.write(fd, input_text.encode())
            except OSError:
                pass
        os.set_blocking(fd, False)
        deadline = time.time() + timeout
        reaped = False
        hit_marker = False
        try:
            while time.time() < deadline:
                r, _, _ = select.select([fd], [], [], 0.5)
                if r:
                    try:
                        chunk = os.read(fd, 65536)
                    except OSError:
                        break
                    if not chunk:
                        break
                    buf += chunk
                    if stop_when and stop_when in buf.decode("utf-8", "replace"):
                        hit_marker = True
                        break
                else:
                    got, _ = os.waitpid(pid, os.WNOHANG)
                    if got:
                        reaped = True
                        break
        finally:
            if not reaped:
                if hit_marker:
                    # 让 libgcov 有机会落盘，再收尸
                    try:
                        os.kill(pid, signal.SIGINT)
                    except OSError:
                        pass
                    end = time.time() + grace
                    while time.time() < end:
                        got, _ = os.waitpid(pid, os.WNOHANG)
                        if got:
                            reaped = True
                            break
                        time.sleep(0.2)
                if not reaped:
                    try:
                        os.kill(pid, signal.SIGKILL)
                    except OSError:
                        pass
            try:
                os.close(fd)
            except OSError:
                pass
        try:
            _, status = os.waitpid(pid, 0)
            code = status >> 8
        except ChildProcessError:
            code = -1

        text = buf.decode("utf-8", "replace")
        # libgcov 在标准错误上打的 "profiling:...:Cannot open" 与业务无关，
        # 留着会让断言里的子串匹配误判。
        text = "\n".join(l for l in text.splitlines() if "profiling:" not in l)
        return code, text

    def _run_driver_detect(self, bus_address: str, cfg_home: Path):
        """在私有 mount namespace 里伪造 NVIDIA 版本后跑 ll-driver-detect。"""
        script = (
            "mount -t tmpfs tmpfs /sys/module 2>/dev/null || exit 0\n"
            "mkdir -p /sys/module/nvidia\n"
            "echo '580.119.02' > /sys/module/nvidia/version\n"
            "chmod 755 /sys/module/nvidia\n"
            "chmod 644 /sys/module/nvidia/version\n"
            f"XDG_CONFIG_HOME={cfg_home} {DRIVER_DETECT_BINARY} 2>&1 "
            "| grep -v 'profiling:' | tail -6\n"
        )
        return self._sudo_on_bus(
            bus_address, "unshare", "-m", "bash", "-c", script,
            timeout=180, check=False)

    # ── Test: 为 PackageManager 服务准备覆盖率收集 ──
    #
    # ⚠️ 没有这一步，服务端的覆盖率【一定会丢】。
    #
    # ll-package-manager 是 systemd 服务，它的进程环境由 systemd 决定，
    # 不会继承冒烟脚本导出的 GCOV_PREFIX。实测（移除 drop-in 后重启）：
    #     服务 Environment 为空 → libgcov 按编译期绝对路径写盘 →
    #     gcda 落到 /tmp/mixrepo/... （73 个文件），
    #     而收集流程只扫 /var/tmp/linglong-cov-{root,user,svc}，
    #     于是安装/卸载/导出 entries 等服务端代码的覆盖率全被忽略。
    #
    # 这里写一个 systemd drop-in 把 GCOV_PREFIX 指定到 svc 前缀。
    # 只在需要收集覆盖率时才有意义，因此做成幂等且可跳过。
    def ensure_service_coverage_env(self) -> bool:
        svc = "org.deepin.linglong.PackageManager.service"
        if not shutil.which("systemctl"):
            return False

        drop_in_dir = Path(
            "/etc/systemd/system/org.deepin.linglong.PackageManager.service.d")
        drop_in = drop_in_dir / "gcov.conf"
        wanted = ("[Service]\n"
                  "Environment=GCOV_PREFIX=/var/tmp/linglong-cov-svc\n"
                  "Environment=GCOV_PREFIX_STRIP=0\n")

        if not drop_in.exists() or drop_in.read_text() != wanted:
            self._run_cmd(["mkdir", "-p", str(drop_in_dir)], sudo=True,
                          check=False)
            # executor 不支持 stdin 输入，用 printf 把内容写进去。
            # 内容里没有 % 与转义字符，直接单引号包裹即可。
            body = wanted.replace("'", "'\\''")
            r = self._run_cmd(
                ["bash", "-c", f"printf '%s' '{body}' > {drop_in}"],
                sudo=True, check=False)
            if r.returncode != 0:
                raise AssertionError(
                    f"写入 {drop_in} 失败: {(r.stdout + r.stderr)[:200]}")
            self._run_cmd(["systemctl", "daemon-reload"], sudo=True,
                          check=False)
            self._run_cmd(["systemctl", "restart", svc], sudo=True,
                          check=False, timeout=300)
            time.sleep(3)

        # 确认服务确实拿到了 GCOV_PREFIX
        r = self._run_cmd(
            ["bash", "-c",
             "tr '\\0' '\\n' < /proc/$(systemctl show -p MainPID --value "
             + svc + ")/environ | grep -c GCOV_PREFIX"],
            sudo=True, check=False)
        counts = [ln.strip() for ln in r.stdout.splitlines()
                  if ln.strip().isdigit()]
        if not counts or int(counts[0]) == 0:
            raise AssertionError(
                "服务仍然没有 GCOV_PREFIX，服务端覆盖率会丢失")

        self.ensure_coverage_prefixes_writable()
        return True

    @staticmethod
    def _uab_utils_missing(result) -> bool:
        """判断这次失败是不是"环境里没有可用的 builder-utils"。

        UAB/UABX 导出需要 cn.org.linyaps.builder.utils，而且
        linglong_builder.cpp:66 要求版本 >= minimumBuilderUtilsVersion
        （当前 deepin/master 是 0.0.4.0）。实测 2026-09 时
        mirror-repo-linglong.deepin.com 与 repo-dev.cicd.getdeepin.org
        【都只发布 0.0.2.0】，所以这一步在纯远程环境里根本做不成，
        只能靠构建缓存里早先留下的高版本副本。

        这属于环境缺件，不是被测功能的问题，所以按"跳过"处理并打清楚
        原因，而不是把整轮冒烟判成 FAIL。等仓库发布了 >= 0.0.4.0 的
        builder-utils，这段会自动重新跑起来。
        """
        combined = (result.stdout or "") + (result.stderr or "")
        markers = (
            "failed to get builder utils",
            "failed to get UAB utilities",
            "builder-utils",
            "builder utils did not provide",
        )
        if not any(m in combined for m in markers):
            return False
        print("    [skip] 环境缺少可用的 builder-utils"
              "（UAB 导出需要 >= 0.0.4.0，仓库只发布 0.0.2.0），"
              "跳过 UAB 相关断言")
        return True

    def ensure_builder_cache_healthy(self) -> bool:
        """修复 ll-builder 构建缓存里"账上有、仓里没有"的 layer 记录。

        背景（踩了很久的坑）：整轮冒烟跑到后半程时，`ll-builder build`
        会稳定失败：

            Build failed: [code -1]:
            stage pull dependency error

        构建其实已经走完依赖解析（base 的 binary/develop 都打印 complete），
        真正的失败点在最后一步 OSTreeRepo::mergeModules()
        （ostree_repo.cpp:2747）。它会对构建仓库里【所有】多模块分组做
        ostree_repo_checkout_at 合并，只要有一个分组的 commit 在仓库里
        已经不存在，整次构建就报这个错（而且是确定性的，重试没用）。

        ⚠️ 关于成因：本函数最初的注释把它归到"`import-dir` 记账但不写
        commit"+"`remove` 只删对象不留记录"，这两句【与代码相反】，
        已按实测更正（2026-09）：
          * OSTreeRepo::importLayerDir（ostree_repo.cpp:819）会
            `commitDirToRepo()` 写出 commit，并把它写进账本 item.commit；
          * OSTreeRepo::remove（ostree_repo.cpp:983）会
            `cache->deleteLayerItem()` 删掉账本记录；对象是随后由
            Builder::remove 的 `if (prune) repo.prune()`
            （linglong_builder.cpp:381）清掉的；
          * 实测：import-dir 之后账本自洽；再触发一次 prune（remove 掉一个
            别的 ref）也不会删掉它的 commit —— 该 commit 有 ostree ref
            （local:main/<id>/<version>/<arch>/<module>）保护。
        也就是说在当前代码上【没能复现】出悬空记录：原始现象要么已被上游
        修掉，要么另有成因。这里保留检查作为【自愈兜底】—— 它很便宜
        （读一个 JSON + 查几个文件是否存在），一旦真出现不自洽就地摘掉，
        而不是让整轮构建在后面随机失败。

        ⚠️ 这里【不能】直接删掉整个 ~/.cache/linglong-builder：
        那样连 cn.org.linyaps.builder.utils 都要重新下载，
        实测会下到 96% 失败，反而让 export --uabx 挂掉（也踩过）。
        所以只把不自洽的记录从 states.json 里摘掉。

        返回 True 表示改过 states.json。
        """
        repo_root = Path.home() / ".cache" / "linglong-builder"
        states = repo_root / "states.json"
        objects = repo_root / "repo" / "objects"
        if not states.exists() or not objects.is_dir():
            return False

        try:
            data = json.loads(states.read_text())
        except (OSError, ValueError):
            return False

        layers = data.get("layers")
        if not isinstance(layers, list):
            return False

        def commit_exists(commit) -> bool:
            if not isinstance(commit, str) or len(commit) < 3:
                return False
            return (objects / commit[:2] / f"{commit[2:]}.commit").exists()

        kept_layers = []
        dropped_commits = set()
        dropped_names = []
        for item in layers:
            commit = (item or {}).get("commit")
            if commit_exists(commit):
                kept_layers.append(item)
                continue
            dropped_commits.add(commit)
            info = (item or {}).get("info") or {}
            dropped_names.append(f"{info.get('id')}/{info.get('version')}"
                                 f"/{info.get('module')}")

        if not dropped_names:
            return False

        print(f"    [warn] 构建缓存不自洽：{len(dropped_names)} 条 layer 记录的 "
              f"commit 在仓库里已不存在，已从 states.json 摘除："
              f"{dropped_names[:3]}")

        # merged 记录里只要引用了被摘掉的 commit 就一并去掉，
        # 否则 mergeModules 会认为"已经合并过"而跳过，后续挂载就会失败。
        merged = data.get("merged")
        if isinstance(merged, list):
            data["merged"] = [
                m for m in merged
                if not (set(m.get("commits") or []) & dropped_commits)
            ]

        data["layers"] = kept_layers
        try:
            states.write_text(json.dumps(data, ensure_ascii=False, indent=2))
        except OSError as exc:
            print(f"    [warn] 写回 states.json 失败: {exc}")
            return False
        return True

    # ── 让各 GCOV_PREFIX 树可写 ──
    #
    # ⚠️ 这是一个【静默丢覆盖率】的坑，实测踩过：
    #
    # libgcov 写 .gcda 时【不会创建中间目录】，它要求整条路径已经存在
    # 且可写。而 GCOV_PREFIX_STRIP=0 会让落盘路径变成
    #   $GCOV_PREFIX + 编译期绝对路径
    # 也就是 $GCOV_PREFIX/home/<user>/.../obj-x86_64-linux-gnu/...
    #
    # 只要这套中间目录是【另一个身份】建出来的（例如以 root 跑过一次，
    # 目录就是 root:root 755），之后换普通用户跑就会在每一个 .gcda 上
    # 报 "profiling:...Cannot open"，覆盖率全部丢弃——而冒烟测试本身
    # 仍然全绿，完全看不出问题。实测一轮日志里有 2879 条 Cannot open。
    #
    # 所以这里按"谁写哪棵树"把属主摆正：
    #   user 前缀 -> 当前用户（非 sudo 的 ll-cli/ll-builder）
    #   svc  前缀 -> deepin-linglong（PackageManager 服务）
    #   root 前缀 -> 保持 root（sudo 调用）
    def ensure_coverage_prefixes_writable(self) -> None:
        me = getpass.getuser()
        plan = (
            ("/var/tmp/linglong-cov-user", f"{me}:{me}"),
            ("/var/tmp/linglong-cov-svc", "deepin-linglong:deepin-linglong"),
            # root 前缀：sudo 命令（root）和 peer 模式的服务端
            # （sudo --user deepin-linglong，见 test_no_dbus_peer_mode）
            # 都会往这里写。这里【不 chown】（里面已经有 root 写的文件），
            # 但必须让所有身份都能写：实测 prefix 里已存在的 gcda 若是
            # root:root 0644，uid 993 的 gcov 覆盖不了它，写入静默失败，
            # peer 分支的覆盖率就永远统计不到。
            # 修前/修后（删掉旧 gcda + 放开权限后重跑一次 peer）：
            #   apps/ll-package-manager/src/main.cpp 未覆盖 57 -> 39 行，
            #   总体 75.00% -> 75.10%。
            ("/var/tmp/linglong-cov-root", None),
        )
        for prefix, owner in plan:
            # 目录不存在时先建出来（chown -R 对不存在的路径会报错）
            self._run_cmd(["mkdir", "-p", prefix], sudo=True, check=False)
            if owner is not None:
                self._run_cmd(["chown", "-R", owner, prefix], sudo=True,
                              check=False)
            # a+rwX：目录可进入、文件可写；X 只作用于目录和已有可执行位，
            # 不会把普通文件误设成可执行。
            self._run_cmd(["chmod", "-R", "a+rwX", prefix], sudo=True,
                          check=False)

    # ── 让 PackageManager 服务把覆盖计数落盘 ──
    #
    # ⚠️ 这是收集服务端覆盖率的【必要步骤】，此前一直缺失。
    #
    # ll-package-manager 是常驻服务（systemd Type=dbus）。libgcov 只在
    # 进程退出时才把 .gcda 写盘，所以服务运行期间执行的所有代码
    # （安装/卸载/导出 entries/延迟卸载等，全在服务里）其计数都还在内存中。
    #
    # 实测（清空 svc prefix 后做一次安装，对比重启前后）：
    #     未重启（计数还在内存里）: 50.8% (9766/19227)
    #     重启服务后（落盘）:       58.2% (11190/19227)
    # 单次操作就相差 1424 行 / 7.4 个百分点 —— 不重启就全丢了。
    # 另一处印证：做完整装操作后 gcda 的 mtime 不变，restart 后立即更新，
    # 且 ostree_repo.cpp 的未覆盖行数从 574 降到 530。
    #
    # 服务需要 GCOV_PREFIX=/var/tmp/linglong-cov-svc（由 systemd drop-in
    # 注入），否则会写到编译期路径而丢失；没有该变量时本函数直接返回
    # False，避免做无用重启。
    def flush_service_coverage(self) -> bool:
        svc = "org.deepin.linglong.PackageManager.service"
        if not shutil.which("systemctl"):
            return False
        # 先确认服务确实带 GCOV_PREFIX，否则重启也没意义。
        # ⚠️ SUDO_ASKPASS 的辅助脚本会把 "验证成功" 一起写进 stdout，
        #    所以不能直接 isdigit()，要按行找一个纯数字行。
        r = self._run_cmd(
            ["bash", "-c",
             "pid=$(systemctl show -p MainPID --value " + svc + "); "
             "tr '\\0' '\\n' < /proc/$pid/environ 2>/dev/null | grep -c GCOV_PREFIX"],
            sudo=True, check=False)
        counts = [ln.strip() for ln in r.stdout.splitlines()
                  if ln.strip().isdigit()]
        if not counts or int(counts[0]) == 0:
            return False

        self._run_cmd(["systemctl", "restart", svc],
                      sudo=True, check=False, timeout=300)
        return True

    def cleanup(self):
        # 先把所有容器收干净再卸载：残留的 ll-box 进程会占住应用引用，
        # 让 sudo ll-cli uninstall 恒返回 255（且没有任何错误输出），
        # 于是应用卸不掉、项目目录删不净，污染下一次运行。
        self._cleanup_containers(DEMO_APP_ID, timeout=30)
        self._cleanup_containers(CALENDAR_APP_ID, timeout=30)
        self._run_cmd([LL_CLI, "kill", "-s", "9", CALENDAR_APP_ID], check=False)
        self._sudo_ll_cli("uninstall", DEMO_APP_ID, check=False)
        self._sudo_ll_cli("uninstall", CALENDAR_APP_ID, check=False)
        self._sudo_ll_cli("uninstall", SEMVER_APP_ID, check=False)
        self._sudo_ll_cli("uninstall", TESTSUITE_BASELINE_APP_ID, check=False)
        self._remove_demo_project_dir()
        self.reset_repositories()

        # ⚠️ 上面这几句会把套件当夹具用的应用卸掉（demo / 日历 / semver /
        # baseline）。但机器上原本就【预装】了其中一些 —— 例如日历，以及它
        # 依赖的 org.deepin.runtime.dtk 和 base —— 卸掉之后它们就回不来了：
        # 实测整轮跑完机器会少 3 个 ref（run52 之后只剩 3 个）。
        # 这里按【整轮开始前的已装集合】兜底还原一次，语义与上面仓库配置的
        # 兜底一致：让机器回到开跑前的样子，而不是留下被清理过的状态。
        # 注意这里【不】判 has_failed：cleanup 卸载夹具是设计如此，还原失败
        # 属于环境问题（装不回来），不该把一个跑得没问题的整轮判失败。
        if self._installed_refs_at_start:
            try:
                self._restore_installed_refs(self._installed_refs_at_start)
            except Exception as exc:  # noqa: BLE001
                print(f"  Error: 还原整轮开始前的已装应用集合失败: {exc}",
                      file=sys.stderr)

        if not self._verify_repo_state_restored("ll-cli"):
            self.has_failed = True
        if not self._verify_repo_state_restored("ll-builder"):
            self.has_failed = True
        # 逐项核对整个仓库配置（镜像开关 / region / 地址 / 优先级…）
        if not self._verify_repo_config_restored():
            # 有步骤漏了还原：这里用整轮开始前的快照【兜底还原】一次，
            # 保证机器不会留在被污染的状态；但这一轮仍然判 FAIL ——
            # "改了配置没还原"是脚本的 bug，必须暴露出来而不是被兜底掩盖。
            print("  Error: 有步骤没还原仓库配置，尝试用开始前的快照兜底还原",
                  file=sys.stderr)
            try:
                self._restore_repo_config(self._repo_config_at_start)
                if self._verify_repo_config_restored():
                    print("  兜底还原成功，机器配置已回到开始前的样子",
                          file=sys.stderr)
            except Exception as exc:  # noqa: BLE001
                print(f"  Error: 兜底还原也失败了: {exc}", file=sys.stderr)
            self.has_failed = True

        # 最后再把服务端计数落盘一次：上面这些卸载/仓库重置同样发生在
        # PackageManager 服务里，不重启就收集不到。
        # 这是整轮冒烟服务端覆盖率的最后一道保障。
        self.flush_service_coverage()

    def _available_versions(self, app_id: str) -> list[str]:
        """按远端给出的顺序返回可用版本（最新的在前）。"""
        r = self._ll_cli("search", "--show-all-version", app_id,
                         timeout=180, check=False)
        out = r.stdout + r.stderr
        seen: list[str] = []
        for v in re.findall(r"\b(\d+\.\d+\.\d+\.\d+)\b", out):
            if v not in seen:
                seen.append(v)
        return seen

    def _require_repo_versions(self, app_id: str, least: int = 1,
                               why: str = "") -> list[str]:
        """确认仓库里还有这个测试包，否则【明确跳过】而不是判失败。

        这些包只用于测试，实测会随镜像同步时有时无。包一没了用例必然
        失败，但那是环境问题：直接跳过并写清原因，比 fail-fast 把后面
        几十个用例全变成 SKIPPED 更有利于定位问题。
        """
        versions = self._available_versions(app_id)
        if len(versions) < least:
            raise StepSkipped(
                f"仓库里已经没有 {app_id}（需要至少 {least} 个版本，"
                f"实际找到 {len(versions)} 个）"
                + (f"：{why}" if why else "")
                + "。这属于环境问题（仓库/镜像里暂时没有这个包），"
                  "不是被测代码的问题")
        return versions

    # ── 桩通知服务（非 TTY 的交互确认）──
    def _try_read_root_file(self, path) -> str:
        """读 root 文件，读不到就当空串（用于"文件可能还没生成"的轮询）。"""
        try:
            return self._read_root_file(path)
        except RuntimeError:
            return ""

    def _sudo_ll_cli_on_bus(self, bus_address: str, *args: str, **kwargs):
        """在指定会话总线上跑 ll-cli。

        ⚠️ 不能用 executor 的 env= 参数：那是 sudo【之前】的环境变量，
           sudo 会把它剥掉。必须在 sudo 之后用 env 显式设置。
        """
        return self._sudo_on_bus(bus_address, LL_CLI, *args, **kwargs)

    def _sudo_on_bus(self, bus_address: str, *cmd: str, **kwargs):
        """以 root 在指定会话总线上跑一条命令。"""
        return self._run_cmd(
            ["env", f"DBUS_SESSION_BUS_ADDRESS={bus_address}", *cmd],
            sudo=True,
            **kwargs,
        )

    def _installed_app_version(self, app_id: str):
        r = self._ll_cli("list", timeout=180, check=False)
        for line in r.stdout.splitlines():
            if app_id in line:
                m = re.search(r"\b(\d+\.\d+\.\d+\.\d+)\b", line)
                if m:
                    return m.group(1)
        return None

    def _start_notification_stub(self, action: str) -> str:
        """起 root 私有会话总线 + 桩通知服务，返回总线地址。"""
        stub = Path(__file__).resolve().parent / "notif_stub.py"
        if not stub.exists():
            raise AssertionError(f"找不到桩通知服务脚本: {stub}")

        probe = self._run_cmd(
            ["python3", "-c",
             "import dbus, dbus.service; from gi.repository import GLib"],
            check=False,
        )
        if probe.returncode != 0:
            raise AssertionError(
                "缺少 python3-dbus / python3-gi，无法起桩通知服务: "
                + (probe.stderr or "").strip()[:200])

        self._stop_notification_stub()
        self._run_cmd(["mkdir", "-p", NOTIF_BUS_DIR], sudo=True, check=False)

        # --fork 让 dbus-daemon 后台化；--print-pid 拿到 PID —— 收尾按 PID 杀，
        # 不用 pkill -f（那条路的模式串会命中 sudo 自己，踩过）
        #
        # ⚠️ --print-address / --print-pid 都要显式写 =1（stdout）：
        #    不写的话前者会把后一个选项当成自己的 fd 参数，
        #    报 Invalid file descriptor: "--print-pid"。
        r = self._run_cmd(
            ["dbus-daemon", "--session",
             f"--address=unix:path={NOTIF_BUS_SOCKET}",
             "--fork", "--print-address=1", "--print-pid=1"],
            sudo=True, check=False,
        )
        address, bus_pid = "", ""
        for line in (r.stdout + r.stderr).splitlines():
            line = line.strip()
            if line.startswith("unix:path="):
                address = line
            elif line.isdigit():
                bus_pid = line
        if r.returncode != 0 or not address or not bus_pid:
            raise AssertionError(
                f"私有会话总线启动失败: rc={r.returncode} "
                f"{(r.stdout + r.stderr)[:300]}")
        self._run_cmd(["bash", "-c", f"echo {bus_pid} > {NOTIF_BUS_PID}"],
                      sudo=True, check=False)

        self._run_cmd(["rm", "-f", NOTIF_STUB_LOG], sudo=True, check=False)
        cmd = (f"DBUS_SESSION_BUS_ADDRESS={address} nohup python3 {stub} "
               f"{action} {NOTIF_STUB_LOG} > {NOTIF_STUB_OUT} 2>&1 & "
               f"echo $! > {NOTIF_STUB_PID}")
        self._run_cmd(["bash", "-c", cmd], sudo=True, check=False)

        # 等桩把 org.freedesktop.Notifications 抢到手（打印 stub ready）
        deadline = time.time() + 15
        out = ""
        while time.time() < deadline:
            out = self._try_read_root_file(NOTIF_STUB_OUT)
            if "stub ready" in out:
                return address
            if "Traceback" in out or "缺少" in out:
                break
            time.sleep(0.5)
        self._stop_notification_stub()
        raise AssertionError(f"桩通知服务没起来: {out[:300]}")

    def _stop_notification_stub(self):
        """按 PID 收掉桩和私有总线（幂等）。"""
        for pid_file in (NOTIF_STUB_PID, NOTIF_BUS_PID):
            r = self._run_cmd(["cat", pid_file], check=False)
            pid = (r.stdout or "").strip()
            if r.returncode == 0 and pid.isdigit():
                self._run_cmd(["kill", pid], sudo=True, check=False)
            self._run_cmd(["rm", "-f", pid_file], sudo=True, check=False)
        self._run_cmd(["rm", "-rf", NOTIF_BUS_DIR], sudo=True, check=False)

    def _notification_log(self) -> list:
        """桩收到的通知，按时间顺序。"""
        notes = []
        for line in self._try_read_root_file(NOTIF_STUB_LOG).splitlines():
            line = line.strip()
            if not line:
                continue
            try:
                notes.append(json.loads(line))
            except json.JSONDecodeError:
                pass
        return notes

    def _run_interaction_round(self, action: str, expect_upgrade: bool,
                               app: str, newest: str, older: str):
        bus = self._start_notification_stub(action)
        try:
            self._sudo_ll_cli("uninstall", app, timeout=300, check=False)
            r = self._sudo_ll_cli("install", f"{app}/{older}",
                                  timeout=NOTIF_INSTALL_TIMEOUT, check=False)
            if r.returncode != 0:
                raise AssertionError(
                    f"[{action}] 安装旧版本 {older} 失败: "
                    f"{(r.stdout + r.stderr)[:300]}")
            got = self._installed_app_version(app)
            if got != older:
                raise AssertionError(
                    f"[{action}] 装旧版后期望 {older}，实际 {got}")

            # 关键一步：装更新的版本 -> Policy::Upgrade -> TaskInteraction
            # -> Cli::interaction -> notifier->request() -> 桩收到通知
            r = self._sudo_ll_cli_on_bus(
                bus, "install", f"{app}/{newest}",
                timeout=NOTIF_INSTALL_TIMEOUT, check=False)
            got = self._installed_app_version(app)

            notes = self._notification_log()
            if not notes:
                raise AssertionError(
                    f"[{action}] 桩没收到通知，说明没走到 dbus_notifier: "
                    f"rc={r.returncode} {(r.stdout + r.stderr)[:200]}")
            note = notes[-1]
            if note.get("summary") != "Package Manager needs to confirm request.":
                raise AssertionError(
                    f"[{action}] 通知标题不对: {note.get('summary')!r}")
            if note.get("actions") != ["yes", "Yes", "no", "No"]:
                raise AssertionError(
                    f"[{action}] 通知按钮不对: {note.get('actions')!r}")
            body = note.get("body", "")
            if older not in body or newest not in body:
                raise AssertionError(
                    f"[{action}] 通知正文没同时提到新旧版本: {body!r}")

            if expect_upgrade:
                if r.returncode != 0:
                    raise AssertionError(
                        f"[{action}] 回答 yes 却失败了: "
                        f"{(r.stdout + r.stderr)[:300]}")
                if got != newest:
                    raise AssertionError(
                        f"[{action}] 回答 yes 后期望 {newest}，实际 {got}")
            else:
                if r.returncode == 0:
                    raise AssertionError(f"[{action}] 回答 no 却装成功了")
                if got != older:
                    raise AssertionError(
                        f"[{action}] 回答 no 后期望仍是 {older}，实际 {got}")
        finally:
            self._stop_notification_stub()

    def _build_with_retry(self, proj, tag, attempts: int = 3):
        """跑 ll-builder build，遇到仓库合并的偶发失败就重试。

        ⚠️ 已知偶发问题：整轮冒烟跑到后半程时，ll-builder build 会随机报

            Build failed: [code -1]:
            stage pull dependency error

        构建已经走完 buildStagePullDependency 的依赖打印（base 的
        binary/develop 都显示 complete），失败点在最后的
        OSTreeRepo::mergeModules() —— 它会对仓库里【所有】多模块分组做
        ostree_repo_checkout_at 合并。整轮跑下来仓库里积累了大量
        探针包，合并项越多越容易撞上偶发失败（同一序列单独跑必过，
        只有整轮跑到这里才会偶发）。

        这是环境/仓库状态问题，不是被测功能的问题，所以这里重试；
        重试仍失败才判 FAIL，并把最后一次的完整输出带出去。
        """
        last = None
        for i in range(attempts):
            last = self._ll_builder("build", cwd=str(proj), timeout=1800,
                                    check=False)
            if last.returncode == 0:
                return last
            combined = last.stdout + last.stderr
            if "stage pull dependency error" not in combined:
                return last
            if i + 1 < attempts:
                print(f"    [warn] {tag} 构建遇到仓库合并偶发失败，"
                      f"重试 {i + 2}/{attempts}")
                time.sleep(10)
        return last

    # ── 会修改机器仓库配置的步骤 ──
    #
    # 这些用例会写 /var/lib/linglong/config.yaml（仓库增删、优先级、
    # 镜像开关、默认仓库…）。那是【机器状态】而不是用例的私有数据：
    # 改了不还原，之后所有运行（包括 Jenkins 上的）都会跑在被污染的
    # 配置上，而且现象很难查。
    #
    # 实测踩过：test_repo_mirror_management 收尾停在 disable-mirror stable，
    # 把机器上原本 mirror_enabled: true 的 stable 关掉了；它还会
    # repo update stable <url> 改地址。这些都不会自己回去。
    #
    # 又踩过一次：test_no_dbus_peer_mode 也会
    # enable-mirror / disable-mirror stable（为了走 peer 模式的写权限
    # 分支），当时没列进来，于是整轮跑完 stable 的镜像开关又被关掉了。
    #
    # 与其在每个用例里手写 try/finally（容易漏），不如在这里声明一次，
    # run_step 会在执行前后自动快照 / 还原，用例里忘了也不会出事；
    # cleanup() 里还有一道兜底还原 + 校验。
    REPO_CONFIG_STEPS = frozenset({
        "test_repo_mirror_management",
        "test_repo_alias_and_mirror",
        "test_no_dbus_peer_mode",
        "test_repo_config_v1_migration",
        "test_repo_cache_rebuild",
        "test_cache_write_failure",
    })
    _REF_ROW = re.compile(r"^(\S+)\s+.*?\s+(\d[\d.]*)\s+(\S+)\s+(\S+)\s",
                          re.M)
