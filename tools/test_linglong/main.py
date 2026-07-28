#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

"""
玲珑冒烟测试 — 主测试类与入口
"""

import os
import random
import re
import subprocess
import sys
import time
import argparse
from datetime import datetime
from pathlib import Path

# ── repository ──
SMOKE_REPO_NAME = "smoketesting"
SMOKE_REPO_URL = "https://repo-dev.cicd.getdeepin.org"

# ── demo app ──
DEMO_APP_ID = "org.deepin.demo"
DEMO_VERSION = "0.0.0.1"
DEMO_ARCH = os.uname().machine
DEMO_CHANNEL = "main"
DEMO_PROJECT_DIR = DEMO_APP_ID

# ── calendar app ──
CALENDAR_APP_ID = "org.dde.calendar"
CALENDAR_VERSION = "5.13.1.1"
CALENDAR_MODULE_BASE_VERSION = "5.14.4.102"
CALENDAR_MODULE_DOWNGRADE_VERSION = "5.14.4.101"

# ── semver ──
SEMVER_APP_ID = "org.deepin.semver.demo"
SEMVER_OLD_VERSION = "1.0.0.0"

# ── baseline testsuite ──
TESTSUITE_BASELINE_APP_ID = "cn.org.linyaps.testsuite.baseline"
TESTSUITE_BASELINE_TIMEOUT = 300

# ── tools ──
LL_CLI = "ll-cli"
LL_BUILDER = "ll-builder"

# ── report ──
RESULTS_FILE = os.path.join(os.getcwd(), "test-results.json")
from .executor import CommandExecutor
from .reporter import generate_report
from .models import RepoState, StepResult

class SmokeTest:

    STEPS = [
        ("清理并重置仓库", "reset_repositories", "仓库管理"),
        (
            "记录 ll-cli 和 ll-builder 当前仓库状态",
            "record_current_repo_state",
            "仓库管理",
        ),
        ("配置冒烟测试仓库", "configure_smoke_repositories", "仓库管理"),
        ("创建 demo 项目", "create_demo_project", "应用构建"),
        ("构建并导出 demo 应用", "test_demo_build_and_export", "应用构建"),
        ("验证 demo DBus 环境变量", "test_demo_dbus_environment", "应用构建"),
        ("安装并运行 demo 应用", "test_demo_install_and_run", "应用部署"),
        ("查询仓库与运行时信息", "test_repository_queries", "应用部署"),
        ("安装、升级并运行日历应用", "test_calendar_install_upgrade_run", "应用管理"),
        ("验证日历模块生命周期", "test_calendar_module_lifecycle", "应用管理"),
        ("验证 versionV1 到 versionV2 升降级", "test_semver_upgrade_flow", "应用管理"),
        ("安装并运行 baseline 测试套件", "test_testsuite_baseline", "应用管理"),
    ]

    def __init__(self, dated: bool = False, verbose: bool = False):
        self.results: list[StepResult] = []
        self.start_time = datetime.now().astimezone()
        self.dated = dated
        self.verbose = verbose
        self.has_failed = False

        self.ll_cli_state = RepoState()
        self.ll_builder_state = RepoState()

        self.executor = CommandExecutor(verbose=verbose)

        # 校验步骤方法名：确保 STEPS 中每个方法都已定义
        for title, method_name, *_ in self.STEPS:
            if not hasattr(self, method_name):
                raise AttributeError(
                    f"Step '{title}' references method '{method_name}' "
                    f"which is not defined on SmokeTest"
                )

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

    def run_step(self, title: str, step_index: int):
        method_name = self.STEPS[step_index][1]
        method = getattr(self, method_name, None)
        if method is None:
            raise AttributeError(f"Test method not found: {method_name}")

        start = time.time_ns()
        print(f"\n==> {title}")
        category = self.STEPS[step_index][2] if len(self.STEPS[step_index]) > 2 else ""
        try:
            method()
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
        except Exception as e:
            elapsed = (time.time_ns() - start) // 1_000_000
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
            self._print_step_result(title, "FAIL")
            self.has_failed = True
            for i in range(step_index + 1, len(self.STEPS)):
                skip_title = self.STEPS[i][0]
                skip_category = self.STEPS[i][2] if len(self.STEPS[i]) > 2 else ""
                self.results.append(
                    StepResult(
                        index=i + 1,
                        title=skip_title,
                        status="SKIPPED",
                        duration_ms=0,
                        category=skip_category,
                    )
                )
            raise

    def run(self):
        try:
            for i in range(len(self.STEPS)):
                title = self.STEPS[i][0]
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

    # ── Test: 清理并重置仓库 ──
    def reset_repositories(self):
        self._sudo_ll_cli("repo", "remove", SMOKE_REPO_NAME, check=False)
        self._ll_builder("repo", "remove", SMOKE_REPO_NAME, check=False)

    # ── Test: 记录当前仓库状态 ──
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

    def record_current_repo_state(self):
        dr, hr, hp = self._parse_repo_show("ll-cli")
        self.ll_cli_state = RepoState(
            default_repo=dr, highest_priority_repo=hr, highest_priority=hp
        )
        print(f"Current ll-cli default repo: {dr}")
        print(f"Current ll-cli highest priority repo: {hr} ({hp})")
        dr, hr, hp = self._parse_repo_show("ll-builder")
        self.ll_builder_state = RepoState(
            default_repo=dr, highest_priority_repo=hr, highest_priority=hp
        )
        print(f"Current ll-builder default repo: {dr}")
        print(f"Current ll-builder highest priority repo: {hr} ({hp})")

    # ── Test: 配置冒烟测试仓库 ──
    def _next_repo_priority(self, current: int) -> int:
        return current + 100

    def configure_smoke_repositories(self):
        p1 = self._next_repo_priority(self.ll_cli_state.highest_priority)
        p2 = self._next_repo_priority(self.ll_builder_state.highest_priority)
        self._sudo_ll_cli("repo", "add", SMOKE_REPO_NAME, SMOKE_REPO_URL)
        self._sudo_ll_cli("repo", "set-priority", SMOKE_REPO_NAME, str(p1))
        self._ll_builder("repo", "add", SMOKE_REPO_NAME, SMOKE_REPO_URL)
        self._ll_builder("repo", "set-priority", SMOKE_REPO_NAME, str(p2))

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

    def create_demo_project(self):
        self._remove_demo_project_dir()
        self._ll_builder("create", DEMO_APP_ID)

    # ── Test: 构建并导出 demo 应用 ──
    def test_demo_build_and_export(self):
        output_file = (
            f"{DEMO_APP_ID}-custom-output-{os.getpid()}-{random.randint(0, 99999)}.uab"
        )
        old_cwd = os.getcwd()
        try:
            os.chdir(DEMO_PROJECT_DIR)
            self._ll_builder("build")
            self._ll_builder("export", "--layer")
            self._ll_builder("export")
            if Path(output_file).exists():
                raise RuntimeError(f"Unexpected file: {output_file}")
            self._ll_builder("export", "--output", output_file)
            if not Path(output_file).is_file():
                raise RuntimeError(f"Expected file not found: {output_file}")
            Path(output_file).unlink(missing_ok=True)
            self._ll_builder("run")
        finally:
            os.chdir(old_cwd)

    # ── Test: 验证 demo DBus 环境变量 ──
    def test_demo_dbus_environment(self):
        session_addr = os.environ.get("DBUS_SESSION_BUS_ADDRESS", "")
        system_addr = os.environ.get(
            "DBUS_SYSTEM_BUS_ADDRESS", "unix:path=/var/run/dbus/system_bus_socket"
        )
        old_cwd = os.getcwd()
        os.chdir(DEMO_PROJECT_DIR)
        try:
            r = self._ll_builder("run", "--", "bash", "-c", "export")
            if "DBUS_SESSION_BUS_ADDRESS" not in r.stdout:
                raise AssertionError("DBUS_SESSION_BUS_ADDRESS not found")
            if "DBUS_SYSTEM_BUS_ADDRESS" not in r.stdout:
                raise AssertionError("DBUS_SYSTEM_BUS_ADDRESS not found")

            env1 = {"DBUS_SESSION_BUS_ADDRESS": f"{session_addr},test=1"}
            r = self._ll_builder("run", "--", "bash", "-c", "export", env=env1)
            if "test=1" not in r.stdout:
                raise AssertionError("DBUS_SESSION_BUS_ADDRESS test=1 not propagated")

            env2 = {"DBUS_SYSTEM_BUS_ADDRESS": f"{system_addr},test=2"}
            r = self._ll_builder("run", "--", "bash", "-c", "export", env=env2)
            if "test=2" not in r.stdout:
                raise AssertionError("DBUS_SYSTEM_BUS_ADDRESS test=2 not propagated")
        finally:
            os.chdir(old_cwd)

    # ── Test: 安装并运行 demo 应用 ──
    def test_demo_install_and_run(self):
        layer_file = f"{DEMO_APP_ID}_{DEMO_VERSION}_{DEMO_ARCH}_binary.layer"
        uab_file = f"{DEMO_APP_ID}_{DEMO_VERSION}_{DEMO_ARCH}_{DEMO_CHANNEL}.uab"
        old_cwd = os.getcwd()
        os.chdir(DEMO_PROJECT_DIR)
        try:
            self._sudo_ll_cli("uninstall", DEMO_APP_ID, check=False)
            self._run_cmd([f"./{uab_file}"])
            self._sudo_ll_cli("install", uab_file)
            self._sudo_ll_cli("uninstall", DEMO_APP_ID, check=False)
            self._sudo_ll_cli("install", layer_file)
            self._run_cmd([LL_CLI, "run", DEMO_APP_ID], timeout=10)
        finally:
            os.chdir(old_cwd)
            self._remove_demo_project_dir()

    # ── Test: 查询仓库与运行时信息 ──
    def test_repository_queries(self):
        self._ll_cli("list")
        self._ll_cli("search", "calendar")
        self._ll_cli("search", "deepin")
        self._ll_cli("search", "deepin", "--type=runtime")

    # ── Test: 安装、升级并运行日历应用 ──
    def test_calendar_install_upgrade_run(self):
        self._sudo_ll_cli("uninstall", CALENDAR_APP_ID, check=False)
        self._sudo_ll_cli("install", CALENDAR_APP_ID)
        self._sudo_ll_cli("uninstall", CALENDAR_APP_ID)
        self._sudo_ll_cli("install", f"{CALENDAR_APP_ID}/{CALENDAR_VERSION}")
        self._sudo_ll_cli("upgrade", CALENDAR_APP_ID)
        proc = subprocess.Popen([LL_CLI, "run", CALENDAR_APP_ID])
        time.sleep(5)
        self._run_cmd([LL_CLI, "kill", "-s", "9", CALENDAR_APP_ID], check=True)
        if proc.poll() is None:
            proc.kill()
            proc.wait()
        time.sleep(3)
        self._sudo_ll_cli("uninstall", CALENDAR_APP_ID)

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

    # ── Test: 日历模块生命周期 ──
    def test_calendar_module_lifecycle(self):
        self._sudo_ll_cli("uninstall", CALENDAR_APP_ID, check=False)
        self._sudo_ll_cli(
            "install", f"{CALENDAR_APP_ID}/{CALENDAR_MODULE_BASE_VERSION}"
        )
        self._sudo_ll_cli("install", "--module", "develop", CALENDAR_APP_ID)
        self._sudo_ll_cli("install", "--module", "unuse", CALENDAR_APP_ID)
        self._sudo_ll_cli("install", "--module", "lang-ja", CALENDAR_APP_ID)
        self._sudo_ll_cli(
            "install",
            "--force",
            f"{CALENDAR_APP_ID}/{CALENDAR_MODULE_DOWNGRADE_VERSION}",
        )
        self._assert_calendar_modules_after_downgrade()
        self._sudo_ll_cli("upgrade", CALENDAR_APP_ID)
        self._assert_calendar_modules_after_upgrade()

    # ── Test: versionV1 到 versionV2 升降级 ──
    def test_semver_upgrade_flow(self):
        self._ll_cli("search", SEMVER_APP_ID)
        self._sudo_ll_cli("uninstall", SEMVER_APP_ID, check=False)
        self._sudo_ll_cli("install", SEMVER_APP_ID)
        r = self._ll_cli("list")
        if SEMVER_APP_ID not in r.stdout:
            raise AssertionError(f"{SEMVER_APP_ID} not found after install")
        self._sudo_ll_cli("uninstall", SEMVER_APP_ID)
        self._sudo_ll_cli("install", f"{SEMVER_APP_ID}/{SEMVER_OLD_VERSION}")
        self._sudo_ll_cli("upgrade", SEMVER_APP_ID)
        r = self._ll_cli("list")
        if SEMVER_APP_ID not in r.stdout:
            raise AssertionError(f"{SEMVER_APP_ID} not found after upgrade")
        self._sudo_ll_cli("uninstall", SEMVER_APP_ID)
        self._sudo_ll_cli("install", "--force", f"{SEMVER_APP_ID}/{SEMVER_OLD_VERSION}")
        r = self._ll_cli("list")
        if SEMVER_APP_ID not in r.stdout:
            raise AssertionError(f"{SEMVER_APP_ID} not found after force install")

    # ── Test: baseline 测试套件 ──
    def test_testsuite_baseline(self):
        self._sudo_ll_cli("uninstall", TESTSUITE_BASELINE_APP_ID, check=False)
        self._sudo_ll_cli("install", TESTSUITE_BASELINE_APP_ID)
        try:
            self._run_cmd(
                [LL_CLI, "run", TESTSUITE_BASELINE_APP_ID],
                timeout=TESTSUITE_BASELINE_TIMEOUT,
                check=True,
            )
        except (RuntimeError, subprocess.CalledProcessError):
            self._sudo_ll_cli("uninstall", TESTSUITE_BASELINE_APP_ID, check=True)
            raise
        self._sudo_ll_cli("uninstall", TESTSUITE_BASELINE_APP_ID)

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

    def cleanup(self):
        self._run_cmd([LL_CLI, "kill", "-s", "9", CALENDAR_APP_ID], check=False)
        self._sudo_ll_cli("uninstall", DEMO_APP_ID, check=False)
        self._sudo_ll_cli("uninstall", CALENDAR_APP_ID, check=False)
        self._sudo_ll_cli("uninstall", SEMVER_APP_ID, check=False)
        self._sudo_ll_cli("uninstall", TESTSUITE_BASELINE_APP_ID, check=False)
        self._remove_demo_project_dir()
        self.reset_repositories()

        if not self._verify_repo_state_restored("ll-cli"):
            self.has_failed = True
        if not self._verify_repo_state_restored("ll-builder"):
            self.has_failed = True


# ── Main ──
def main():
    parser = argparse.ArgumentParser(description="玲珑冒烟测试")
    parser.add_argument(
        "--dated", action="store_true", help="Append timestamp to output filename"
    )
    parser.add_argument(
        "--verbose",
        "-v",
        action="store_true",
        help="Print detailed error output for failed commands",
    )
    args = parser.parse_args()

    print("==========================================")
    print("  玲珑冒烟测试")
    print("==========================================")

    test = SmokeTest(dated=args.dated, verbose=args.verbose)
    test.run()

    if test.has_failed:
        print("\nSmoke testing failed")
        sys.exit(1)
    else:
        print("\n成功执行玲珑冒烟测试")
        sys.exit(0)


if __name__ == "__main__":
    main()
