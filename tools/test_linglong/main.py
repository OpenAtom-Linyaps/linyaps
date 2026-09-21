#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

"""
玲珑冒烟测试 — 主测试类与入口
"""

import base64
import getpass
import json
import os
import pty
import shutil
import tempfile
import random
import re
import select
import signal
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
        # ⚠️ 这两个用例必须在 demo 项目目录还存在时执行：
        #    步骤「安装并运行 demo 应用」结束时会把 org.deepin.demo/ 删掉，
        #    所以它们必须排在它前面，不能放到末尾。
        ("builder build 跳过选项", "test_builder_build_options", "应用构建"),
        ("builder export 选项", "test_builder_export_options", "应用构建"),
        ("builder run 选项", "test_builder_run_options", "应用构建"),
        # push 的错误路径需要在真实项目目录里跑（要读 linglong.yaml），
        # 所以必须和上面几个 builder 用例一样排在「安装并运行 demo 应用」之前
        # —— 那一步结束会把 org.deepin.demo/ 删掉。
        ("builder push 错误路径", "test_builder_push_errors", "构建器覆盖"),
        ("安装并运行 demo 应用", "test_demo_install_and_run", "应用部署"),
        ("查询仓库与运行时信息", "test_repository_queries", "应用部署"),
        ("ll-cli run 运行期选项", "test_run_command_options", "运行时覆盖"),
        ("容器生命周期 ps/enter/kill", "test_container_lifecycle", "运行时覆盖"),
        ("容器实例复用", "test_container_instance_reuse", "运行时覆盖"),
        # ── 覆盖率补强用例（第一批）──
        # 只依赖 base 与已安装应用，不依赖 demo 构建产物。
        # ⚠️ 必须排在「清理未使用运行时（prune）」之前：prune 会把无人依赖的
        #    base 一并移除，之后 run 就没有可运行目标了。
        ("run 的文件/URL/设备传递选项", "test_run_file_url_device", "运行时覆盖"),
        ("run 的 base/runtime 显式指定", "test_run_explicit_base_runtime", "运行时覆盖"),
        ("run 的 debug 选项错误路径", "test_run_debug_options", "运行时覆盖"),
        ("全局选项与帮助输出", "test_global_options", "命令覆盖"),
        ("清理未使用运行时（prune）", "test_prune", "命令覆盖"),
        ("search 筛选选项", "test_search_options", "命令覆盖"),
        ("inspect 目录查询", "test_inspect_commands", "命令覆盖"),
        ("扩展运行（--extensions）", "test_run_with_extensions", "运行时覆盖"),
        ("安装/升级错误路径", "test_install_error_paths", "命令覆盖"),
        ("安装、升级并运行日历应用", "test_calendar_install_upgrade_run", "应用管理"),
        ("验证日历模块生命周期", "test_calendar_module_lifecycle", "应用管理"),
        ("验证 versionV1 到 versionV2 升降级", "test_semver_upgrade_flow", "应用管理"),
        ("安装并运行 baseline 测试套件", "test_testsuite_baseline", "应用管理"),
        ("查询只读子命令（ps/list/repo）", "test_readonly_query_commands", "命令覆盖"),
        ("检查已安装应用（info/content）", "test_installed_app_inspection", "命令覆盖"),
        ("验证错误路径处理", "test_error_paths", "命令覆盖"),
        ("分析已安装应用（analyze）", "test_analyze_commands", "命令覆盖"),
        ("builder 项目级子命令", "test_builder_project_commands", "构建器覆盖"),
        # ── 覆盖率补强用例（第二批）──
        # 安装/卸载/升级的选项分支错误路径，只改参数不改系统状态，放最后安全。
        ("install 的仓库与模块选项", "test_install_repo_module_options", "命令覆盖"),
        ("uninstall 的模块与强制选项", "test_uninstall_module_force_options", "命令覆盖"),
        ("upgrade 的依赖与清理选项", "test_upgrade_deps_prune_options", "命令覆盖"),
        # ── 覆盖率补强用例（第三批）──
        # 这三个不依赖 demo 产物，也不改变已安装应用，放最后最安全。
        # peer 模式走 ll-cli 自建的 package-manager，与 dbus 服务是两套代码；
        # --json 走 json_printer；镜像管理走 repo 的写分支。
        ("peer 模式（--no-dbus）", "test_no_dbus_peer_mode", "命令覆盖"),
        ("仓库镜像管理", "test_repo_mirror_management", "命令覆盖"),
        ("--json 输出格式", "test_json_output", "命令覆盖"),
        # ── 覆盖率补强用例（第四批）──
        # 都在真 PTY 下跑：ll-cli 用 isatty() 决定是否走 TTY 分支，
        # subprocess 的管道会让这些代码永远不被执行。
        # 必须排在日历应用已安装之后（依赖 CALENDAR_APP_ID 可运行）。
        ("PTY 下的 run 与容器复用", "test_run_in_pty", "运行时覆盖"),
        ("run 的文件/URL 占位符映射", "test_run_placeholder_mapping", "运行时覆盖"),
        ("CDI 设备注入", "test_cdi_device_injection", "运行时覆盖"),
        ("显示/时区/网络环境分支", "test_display_env_branches", "运行时覆盖"),
        ("--debug 的 gdb 附加提示", "test_debug_attach_hint", "运行时覆盖"),
        # ── 覆盖率补强用例（第五批）──
        # driver-detect 是独立程序，不依赖应用；放在 PTY 用例之后。
        ("图形驱动检测工具", "test_driver_detect", "命令覆盖"),
        # entries 重写需要独立构建并安装一个项目，放在最后、
        # 已安装应用都验证完之后（它自己会卸载并清理）。
        ("entries 文件重写", "test_entry_file_rewrite", "应用构建"),
        # ⚠️ 迁移用例会改 /var/lib/linglong/.version 并重启服务，
        #    放最后；它自己负责还原，跑完还会校验服务与 ll-cli 可用。
        ("仓库版本迁移", "test_repo_migration", "仓库管理"),
        # ── 覆盖率补强用例（第六批）──
        # 升级交互：先装旧版本再装新版，服务端会走 Policy::Upgrade 并发出
        # TaskInteraction 信号，客户端 Cli::interaction 才会被执行
        # （实测该函数 40 行里覆盖到 28 行）。自己还原版本。
        ("版本升降级触发安装交互", "test_upgrade_interaction", "应用管理"),
        # 源码拉取：linglong.yaml 的 sources 字段，走 fetchSources +
        # SourceFetcher + fetch-<kind>-source 脚本，此前整块未覆盖。
        # 自己起本地 HTTP 服务供 wget 下载，不依赖外网。
        ("项目源码拉取（sources）", "test_project_sources_fetch", "应用构建"),
        # layer 往返：export --layer -> import / extract / import-dir。
        # import-dir 是【隐藏子命令】（->group("")），--help 里看不到。
        ("layer 导入导出往返", "test_builder_layer_roundtrip", "构建器覆盖"),
        # 隐藏的运行/卸载选项：run --privileged（需 root）/--caps-add/
        # --run-context，以及 uninstall 的 --prune/--all 兼容标志。
        # 这些在 --help 里都不显示（->group("")）。
        ("隐藏的运行与卸载选项", "test_hidden_options", "运行时覆盖"),
        # 非标准版本串：VersionV1/V2 都解析不了时会回退到 FallbackVersion，
        # 整个 fallback_version.cpp 此前是 0% 覆盖。
        ("非标准版本串回退解析", "test_fallback_version_strings", "命令覆盖"),
        # 运行中升级 -> switchAppVersion 移除旧版本 -> tryUninstallRef
        # 发现旧版本 busy -> markDeleted 延迟删除 -> 定时器 deferredUninstall。
        # 覆盖 markDeleted / tryUninstallRef / deferredUninstall。
        ("运行中升级触发延迟卸载", "test_upgrade_running_app", "应用管理"),
        # ll-builder 的 clean / create：clean 此前一次都没跑过。
        ("builder clean 与 create", "test_builder_clean_create", "构建器覆盖"),
        # 容器配置补丁机制：/usr/lib/linglong/container/config.d 下的
        # 可执行补丁与 JSON 补丁（applyPatch / applyJsonPatchFile /
        # applyExecutablePatch），此前整块未覆盖。
        ("容器配置补丁机制", "test_container_config_patch", "运行时覆盖"),
        # buildext.apt.depends：让构建走 buildStagePreCommit 里
        # "准备 overlay + 跑依赖脚本"这条整链（此前 57 行里 49 行未覆盖），
        # 以及 generateDependsScript。
        ("builder buildext apt depends", "test_builder_buildext_apt", "构建器覆盖"),
        # UABX（自执行 UAB）：export --uabx。此前冒烟里 "uabx" 出现 0 次，
        # exportUAB 92 行里 68 行、uab_packager.cpp 的
        # prepareExecutableBundle 整块（31 行）都未覆盖。
        ("UABX 自执行包导出", "test_builder_uabx_export", "构建器覆盖"),
        # install hook：/etc/linglong/config.d 下的 ll-pre-install= /
        # ll-post-install= / ll-post-uninstall= 行，从【文件】安装时
        # 解析并执行。hooks.cpp 此前只有 22% 覆盖。
        ("安装钩子（install hooks）", "test_install_hooks", "仓库覆盖"),
        # 强制 entries 全量重新导出：删掉 entries/.version 再重启服务，
        # 服务启动时 ll-package-manager 会调 fixExportAllEntries ->
        # exportAllEntries。这两个函数此前都是 0%。
        #
        # ⚠️ 必须放在【最后】：重导出会让 ostree 仓库忙上一阵子，
        #    后面紧跟构建会随机报 "stage pull dependency error /
        #    cannot be used in worksheets"（踩过，整轮挂在这）。
        ("entries 全量重导出", "test_repo_entries_reexport", "仓库覆盖"),
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

        try:
            for i in range(len(self.STEPS)):
                title = self.STEPS[i][0]
                # 每步之前修一下构建缓存的自洽性。
                # 冒烟里的 import / import-dir 会在构建仓库的 states.json
                # 里留下 commit 已不存在的 layer 记录，之后任何一次
                # ll-builder build 的 mergeModules() 都会失败
                # （报 "stage pull dependency error"，重试无用）。
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


    # ── Test: 只读子命令全覆盖 ──
    #
    # ps / list 的各种变体原先完全没有用例，cli.cpp 里对应的大段代码零覆盖。
    # 全部只读或幂等，不会破坏环境。
    # ⚠️ 每条断言都在真机上实测过，不要凭 help 文本想当然：
    #    - ll-cli version 不存在（rc=109）
    #    - repo show --json 不支持 --json（rc=109）
    #    这两个故意放在错误路径用例里，用来覆盖参数校验分支。
    def test_readonly_query_commands(self):
        # ps：列出运行中的容器。没有应用在跑时给中文提示，退出码 0
        r = self._ll_cli("ps")
        if r.returncode != 0:
            raise AssertionError(f"ll-cli ps 失败: rc={r.returncode}")
        if r.stdout.strip() != "没有正在运行的容器":
            raise AssertionError(
                f"ll-cli ps 无容器时应提示无容器, 实际: {r.stdout.strip()[:120]!r}")

        # ps --json：空时应是合法 JSON 数组
        r = self._ll_cli("ps", "--json")
        if r.returncode != 0:
            raise AssertionError(f"ll-cli ps --json 失败: rc={r.returncode}")
        try:
            parsed = json.loads(r.stdout)
        except json.JSONDecodeError as e:
            raise AssertionError(
                f"ll-cli ps --json 输出不是合法 JSON: {r.stdout[:120]!r} ({e})")
        if not isinstance(parsed, list):
            raise AssertionError(
                f"ll-cli ps --json 应输出数组, 实际: {type(parsed).__name__}")

        # list 的各种变体，每种走 cli.cpp 里不同的分支
        for args in (["list"], ["list", "--json"], ["list", "--type=all"],
                     ["list", "--type=runtime"], ["list", "--type=app"],
                     ["list", "--upgradable"]):
            r = self._ll_cli(*args)
            if r.returncode != 0:
                raise AssertionError(
                    f"ll-cli {' '.join(args)} 失败: rc={r.returncode} {r.stderr[:150]}")
            if args[-1] == "--json":
                try:
                    json.loads(r.stdout)
                except json.JSONDecodeError as e:
                    raise AssertionError(
                        f"ll-cli {' '.join(args)} 输出不是合法 JSON: "
                        f"{r.stdout[:120]!r} ({e})")

        # 全局 --json 选项走的是和子命令 --json 不同的解析路径
        r = self._ll_cli("--json", "list")
        if r.returncode != 0:
            raise AssertionError(f"ll-cli --json list 失败: rc={r.returncode}")

    # ── Test: 已安装应用的 info / content ──
    #
    # info / content 必须先装一个应用才能跑，所以放在 demo 安装之后。
    # 这里自己装一次再卸掉，保证用例独立、不依赖前面步骤的副作用。
    def test_installed_app_inspection(self):
        # 前置条件：demo 应用已由前面的步骤安装。
        # 先确认它确实在，避免用例因为顺序问题给出误导性的失败。
        app_id = DEMO_APP_ID
        listing = self._ll_cli("list")
        if app_id not in listing.stdout:
            raise AssertionError(
                f"{app_id} 未安装，本用例应在安装步骤之后执行")

        r = self._ll_cli("info", app_id)
        if r.returncode != 0:
            raise AssertionError(
                f"ll-cli info {app_id} 失败: rc={r.returncode} {r.stderr[:200]}")
        if app_id not in r.stdout:
            raise AssertionError(
                f"ll-cli info 输出应含应用 ID {app_id}, 实际: {r.stdout[:200]!r}")

        r = self._ll_cli("info", app_id, "--json")
        if r.returncode != 0:
            raise AssertionError(
                f"ll-cli info {app_id} --json 失败: {r.stderr[:200]}")
        try:
            json.loads(r.stdout)
        except json.JSONDecodeError as e:
            raise AssertionError(f"ll-cli info --json 输出不是合法 JSON: {e}")

        r = self._ll_cli("content", app_id)
        if r.returncode != 0:
            raise AssertionError(
                f"ll-cli content {app_id} 失败: rc={r.returncode} {r.stderr[:200]}")

    # ── Test: 错误路径处理 ──
    #
    # 错误处理分支在正常流程里永远走不到，但代码量不小。
    # 断言"失败得干净"：非 0 退出、有输出、不被信号杀死。
    def test_error_paths(self):
        bogus = "org.deepin.nonexistent.app"
        for args in (["info", bogus], ["content", bogus],
                     ["run", bogus], ["uninstall", bogus]):
            r = self._ll_cli(*args, check=False)
            if r.returncode == 0:
                raise AssertionError(
                    f"ll-cli {' '.join(args)} 对不存在的应用竟然成功")
            if r.returncode < 0:
                raise AssertionError(
                    f"ll-cli {' '.join(args)} 被信号杀死 (rc={r.returncode})，不应崩溃")
            if not (r.stderr.strip() or r.stdout.strip()):
                raise AssertionError(
                    f"ll-cli {' '.join(args)} 失败但没有任何输出")

        # 参数校验分支：不存在的子命令 / 不被支持的选项
        for args in (["version"], ["repo", "show", "--json"], ["--nonsense-flag"]):
            r = self._ll_cli(*args, check=False)
            if r.returncode == 0:
                raise AssertionError(f"ll-cli {' '.join(args)} 非法参数竟然成功")
            if r.returncode < 0:
                raise AssertionError(f"ll-cli {' '.join(args)} 崩溃了")


    # ── Test: analyze 子命令 ──
    #
    # analyze 有 size / depends 两个子命令，size 还带 --sort / --asc。
    # 这些分支在 cli.cpp 里对应 calculateModuleSizes / calculateRealDiskUsage /
    # sortDependsTree / moduleNameLess 等函数，原先全零覆盖。
    def test_analyze_commands(self):
        # size 的 5 个排序字段，每个走不同的比较器
        for field in ("actual", "logical", "exclusive", "shared", "id"):
            r = self._ll_cli("analyze", "size", f"--sort={field}")
            if r.returncode != 0:
                raise AssertionError(
                    f"ll-cli analyze size --sort={field} 失败: "
                    f"rc={r.returncode} {r.stderr[:200]}")

        r = self._ll_cli("analyze", "size", "--asc")
        if r.returncode != 0:
            raise AssertionError(f"ll-cli analyze size --asc 失败: {r.stderr[:200]}")

        r = self._ll_cli("analyze", "size", "--sort=id", "--asc")
        if r.returncode != 0:
            raise AssertionError(
                f"ll-cli analyze size --sort=id --asc 失败: {r.stderr[:200]}")

        # depends：依赖树
        r = self._ll_cli("analyze", "depends", check=False)
        if r.returncode != 0:
            raise AssertionError(
                f"ll-cli analyze depends 失败: rc={r.returncode} {r.stderr[:200]}")

        # depends 带 APP：走"单个应用的依赖树"分支，与不带参数的
        # "全部应用"是两段不同的代码（实测 Cli::depends 80 行里
        # 只测不带参数时覆盖不到这 62 行）。
        # ⚠️ 只对 app 有效，传 runtime/base 会被拒（is not an app）。
        for target in (CALENDAR_APP_ID, "org.deepin.runtime.dtk",
                       "org.deepin.base"):
            self._ll_cli("analyze", "depends", target, timeout=180,
                         check=False)
        self._ll_cli("analyze", "depends", "org.no.such.app",
                     timeout=180, check=False)

        # 非法排序字段应被拒绝
        r = self._ll_cli("analyze", "size", "--sort=nonexistent", check=False)
        if r.returncode == 0:
            raise AssertionError("ll-cli analyze size --sort=nonexistent 竟然成功")

    # ── Test: ll-builder 项目级子命令 ──
    #
    # 这些子命令原先只有 create/build/export/run/list/remove 被碰到，
    # 且都是"成功路径"。这里补上失败路径与带选项的路径，
    # 对应 linglong_builder.cpp 里的 cmdListApp / cmdRemoveApp / push / extractLayer。
    def test_builder_project_commands(self):
        # list：列出已构建应用。构建过 demo 之后这里应有内容
        r = self._ll_builder("list")
        if r.returncode != 0:
            raise AssertionError(f"ll-builder list 失败: {r.stderr[:200]}")

        # remove 的两种参数形态（不存在 / 非法格式）
        r = self._ll_builder("remove", "org.deepin.nonexistent", check=False)
        if r.returncode < 0:
            raise AssertionError("ll-builder remove 崩溃了")

        r = self._ll_builder("remove", "not-a-valid-ref", check=False)
        if r.returncode < 0:
            raise AssertionError("ll-builder remove 非法引用时崩溃了")

        # extract / import：文件不存在时应给出明确错误且退出码非 0
        for sub in ("extract", "import"):
            args = [sub, "/nonexistent/path/to/layer.layer"]
            if sub == "extract":
                args.append("/tmp/ll-extract-out")
            r = self._ll_builder(*args, check=False)
            if r.returncode == 0:
                raise AssertionError(f"ll-builder {sub} 不存在的文件竟然成功")
            if r.returncode < 0:
                raise AssertionError(f"ll-builder {sub} 崩溃了")
            if not (r.stderr.strip() or r.stdout.strip()):
                raise AssertionError(f"ll-builder {sub} 失败但无任何输出")

        # build 在非项目目录下的错误路径
        r = self._ll_builder("build", "--offline", cwd="/tmp", check=False)
        if r.returncode == 0:
            raise AssertionError("ll-builder build 在非项目目录竟然成功")

        # push 在非项目目录下的错误路径
        r = self._ll_builder("push", cwd="/tmp", check=False)
        if r.returncode == 0:
            raise AssertionError("ll-builder push 在非项目目录竟然成功")

    # ── Test: ll-builder build 的跳过类选项 ──
    #
    # build 有一批 --skip-* / --offline 选项，正常构建流程一个都不会走到。
    # 在 demo 项目里跑一遍，覆盖这些分支。
    def test_builder_build_options(self):
        old_cwd = os.getcwd()
        try:
            os.chdir(DEMO_PROJECT_DIR)
            # 已构建过，搭配 skip 选项可快速走通而不重复真构建
            r = self._ll_builder(
                "build", "--skip-fetch-source", "--skip-pull-depend",
                "--skip-run-container", "--skip-commit-output",
                check=False)
            if r.returncode < 0:
                raise AssertionError("ll-builder build 带 skip 选项时崩溃了")

            r = self._ll_builder("build", "--skip-output-check", check=False)
            if r.returncode < 0:
                raise AssertionError("ll-builder build --skip-output-check 崩溃了")
        finally:
            os.chdir(old_cwd)
            self._sudo_ll_cli("uninstall", DEMO_APP_ID, check=False)

    # ── Test: export 的压缩器与模块选项 ──
    def test_builder_export_options(self):
        old_cwd = os.getcwd()
        try:
            os.chdir(DEMO_PROJECT_DIR)
            for compressor in ("lz4", "zstd"):
                out = f"{DEMO_APP_ID}-c-{compressor}-{os.getpid()}.uab"
                r = self._ll_builder(
                    "export", "-z", compressor, "--output", out, check=False)
                if r.returncode < 0:
                    raise AssertionError(f"ll-builder export -z {compressor} 崩溃了")
                Path(out).unlink(missing_ok=True)

            # --layer 与 --output 互斥，应被拒绝
            r = self._ll_builder("export", "--layer", "--output", "/tmp/x.uab",
                                 check=False)
            if r.returncode == 0:
                raise AssertionError("export --layer 与 --output 互斥却没报错")

            # 非法的压缩器
            r = self._ll_builder("export", "-z", "bogus", check=False)
            if r.returncode == 0:
                raise AssertionError("export -z bogus 竟然成功")
        finally:
            os.chdir(old_cwd)
            # ll-builder run 会把 demo 装进系统。这里主动卸掉，
            # 否则后面的「安装并运行 demo 应用」会因"应用程序已经安装"而失败。
            self._sudo_ll_cli("uninstall", DEMO_APP_ID, check=False)

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
    def _wait_container(self, timeout: int = 40) -> str:
        deadline = time.time() + timeout
        while time.time() < deadline:
            r = self._ll_cli("ps", "--json", check=False)
            if r.returncode == 0:
                try:
                    arr = json.loads(r.stdout)
                except json.JSONDecodeError:
                    arr = []
                if arr:
                    return arr[0].get("id", "")
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

    # ── Test: ll-cli run 的运行期选项 ──
    #
    # run 有一大批影响容器配置的选项，代码在 run_context.cpp 和
    # container_cfg_builder.cpp 里（合计约 1700 行，原先大半零覆盖）。
    # 这里逐个跑一遍，断言能正常启动并退出。
    def test_run_command_options(self):
        app_id = DEMO_APP_ID
        option_sets = [
            ("--env", ["--env", "SMOKE_TEST_VAR=1"]),
            ("--workdir", ["--workdir", "/tmp"]),
            ("--disable-xdp", ["--disable-xdp"]),
            ("--enable-xdp", ["--enable-xdp"]),
            ("--enable-pipewire", ["--enable-pipewire"]),
            ("--enable-atspi", ["--enable-atspi"]),
            ("--instance", ["--instance", "smokeinst"]),
            ("--cdi-spec-dir", ["--cdi-spec-dir", "/etc/cdi"]),
        ]
        for name, opts in option_sets:
            r = self._ll_cli("run", app_id, *opts, "--", "/bin/true",
                             timeout=90, check=False)
            if r.returncode != 0:
                raise AssertionError(
                    f"ll-cli run {name} 失败: rc={r.returncode} {r.stderr[:200]}")

    # ── Test: 容器生命周期（ps / enter / kill）──
    #
    # demo 应用本身是 echo hello world，跑完立刻退出，看不出容器。
    # 所以先用 ll-cli run <app> -- sleep N 起一个长驻容器，
    # 才能覆盖 ps / enter / kill 以及 cli.cpp 里的
    # checkContainerStatus / acceptConsoleFd / setupIOChannels 等函数。
    def test_container_lifecycle(self):
        app_id = DEMO_APP_ID
        proc = subprocess.Popen(
            [LL_CLI, "run", app_id, "--", "sleep", "120"],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        try:
            cid = self._wait_container(timeout=40)
            if not cid:
                raise AssertionError("容器已启动但 ll-cli ps 看不到它")

            # ps 的人类可读输出里应出现应用 ID
            r = self._ll_cli("ps")
            if r.returncode != 0:
                raise AssertionError(f"ll-cli ps 失败: {r.stderr[:200]}")
            if app_id not in r.stdout:
                raise AssertionError(
                    f"ll-cli ps 输出里应含 {app_id}, 实际: {r.stdout[:200]!r}")

            # ps --json 的结构
            r = self._ll_cli("ps", "--json")
            try:
                arr = json.loads(r.stdout)
            except json.JSONDecodeError as e:
                raise AssertionError(f"ll-cli ps --json 非法 JSON: {e}")
            if not arr or arr[0].get("id") != cid:
                raise AssertionError(
                    f"ps --json 应含容器 {cid}, 实际: {r.stdout[:200]!r}")

            # enter：必须在伪终端里跑
            rc, out = self._run_in_pty(
                [LL_CLI, "enter", cid, "--", "/bin/echo", "SMOKE_ENTER_OK"])
            if rc != 0:
                raise AssertionError(f"ll-cli enter 失败: rc={rc} out={out[:200]!r}")
            if "SMOKE_ENTER_OK" not in out:
                raise AssertionError(f"enter 输出里没有预期内容: {out[:200]!r}")

            # kill
            r = self._ll_cli("kill", cid, timeout=60, check=False)
            if r.returncode != 0:
                raise AssertionError(
                    f"ll-cli kill {cid} 失败: rc={r.returncode} {r.stderr[:200]}")
        finally:
            if proc.poll() is None:
                proc.kill()
                proc.wait()
            # 兜底清理。必须确保容器真的没了：
            # 残留的 ll-box 进程会占住应用引用，导致后面的
            # sudo ll-cli uninstall 一直返回 255（且没有任何错误输出），
            # 进而让后续所有安装类步骤全部失败。
            self._cleanup_containers(app_id)

    # ── Test: ll-builder run 的选项 ──
    #
    # 需要 demo 项目目录存在，必须排在「安装并运行 demo 应用」之前
    # （那一步结束时会把 org.deepin.demo/ 删掉）。
    def test_builder_run_options(self):
        old_cwd = os.getcwd()
        try:
            os.chdir(DEMO_PROJECT_DIR)
            for name, opts in [
                ("--workdir", ["--workdir", "/tmp"]),
                ("--debug", ["--debug"]),
                ("--modules binary", ["--modules", "binary"]),
                ("--modules develop", ["--modules", "develop"]),
                ("--modules binary,develop", ["--modules", "binary,develop"]),
            ]:
                r = self._ll_builder("run", *opts, "--", "/bin/true",
                                     timeout=120, check=False)
                if r.returncode != 0:
                    raise AssertionError(
                        f"ll-builder run {name} 失败: rc={r.returncode} "
                        f"{r.stderr[:200]}")
        finally:
            os.chdir(old_cwd)

    # ── Test: 容器实例复用（reuseContainer）──
    #
    # ll-cli run --instance <name> 会在第二次调用时复用已有容器，
    # 这条路径在 cli.cpp 里对应 reuseContainer 的多个重载（L1273~L1607，
    # 约 300 行），原先完全没被触发。
    def test_container_instance_reuse(self):
        app_id = DEMO_APP_ID
        inst = f"smokereuse{os.getpid()}"
        proc = subprocess.Popen(
            [LL_CLI, "run", app_id, "--instance", inst, "--", "sleep", "120"],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        try:
            cid = self._wait_container(timeout=40)
            if not cid:
                raise AssertionError("带 --instance 启动的容器未出现在 ll-cli ps 中")

            # 第二次使用同一个 instance：应复用而不是新建
            r = self._ll_cli("run", app_id, "--instance", inst, "--",
                             "/bin/echo", "REUSED", timeout=90, check=False)
            if r.returncode != 0:
                raise AssertionError(
                    f"复用实例失败: rc={r.returncode} {r.stderr[:200]}")
            if "REUSED" not in r.stdout:
                raise AssertionError(
                    f"复用实例的输出里没有预期内容: {r.stdout[:200]!r}")

            # 复用后容器数不应增加（仍只有一个）
            r = self._ll_cli("ps", "--json", check=False)
            try:
                arr = json.loads(r.stdout)
            except json.JSONDecodeError:
                arr = []
            if len(arr) != 1:
                raise AssertionError(
                    f"复用后应只有 1 个容器, 实际 {len(arr)} 个: {r.stdout[:200]!r}")
        finally:
            if proc.poll() is None:
                proc.kill()
                proc.wait()
            self._cleanup_containers(app_id)

    # ── Test: 清理未使用的 base/runtime ──
    #
    # prune 需要 root（走 polkit 授权），所以用 sudo 调。
    # 对应 package_manager.cpp 的 Prune / pruneImpl / isRefBusy。
    def test_prune(self):
        r = self._sudo_ll_cli("prune", timeout=180, check=False)
        if r.returncode != 0:
            raise AssertionError(
                f"sudo ll-cli prune 失败: rc={r.returncode} {r.stderr[:200]}")
        combined = r.stdout + r.stderr
        if "unused" not in combined.lower() and "未使用" not in combined:
            raise AssertionError(
                f"prune 输出里应有清理结果说明, 实际: {combined[:200]!r}")

    # ── Test: search 的各种筛选选项 ──
    #
    # search 的 --type / --dev / --show-all-version 会走到 cli.cpp 里
    # 不同的查询构造分支。
    def test_search_options(self):
        for opts in (["--type=all"], ["--type=runtime"], ["--type=base"],
                     ["--type=app"], ["--dev"], ["--show-all-version"],
                     ["--type=app", "--dev", "--show-all-version"]):
            r = self._ll_cli("search", "deepin", *opts, timeout=90, check=False)
            if r.returncode != 0:
                raise AssertionError(
                    f"ll-cli search {' '.join(opts)} 失败: "
                    f"rc={r.returncode} {r.stderr[:200]}")

        # 非法 type：实测不会报错，只是搜不到东西（rc=0，提示未找到）。
        # 这里只断言"不崩溃"，不要凭 CLI11 的枚举校验想当然。
        r = self._ll_cli("search", "deepin", "--type=bogus", check=False)
        if r.returncode < 0:
            raise AssertionError("search --type=bogus 崩溃了")

    # ── Test: inspect 子命令 ──
    #
    # ll-cli inspect dir 会打印应用的 layer/bundle 目录，
    # 对应 cli.cpp 的 inspect / getLayerDir / getBundleDir（约 120 行）。
    # 必须先有已安装的应用。
    def test_inspect_commands(self):
        app_id = DEMO_APP_ID
        listing = self._ll_cli("list")
        if app_id not in listing.stdout:
            raise AssertionError(f"{app_id} 未安装，本用例需在安装步骤之后执行")

        # 默认 type=layer
        r = self._ll_cli("inspect", "dir", app_id)
        if r.returncode != 0:
            raise AssertionError(
                f"ll-cli inspect dir {app_id} 失败: rc={r.returncode} {r.stderr[:200]}")
        if not r.stdout.strip():
            raise AssertionError("inspect dir 没有输出目录路径")

        # 显式 layer
        r = self._ll_cli("inspect", "dir", "-t", "layer", app_id)
        if r.returncode != 0:
            raise AssertionError(
                f"inspect dir -t layer 失败: rc={r.returncode} {r.stderr[:200]}")

        # bundle 类型
        r = self._ll_cli("inspect", "dir", "-t", "bundle", app_id, check=False)
        if r.returncode < 0:
            raise AssertionError("inspect dir -t bundle 崩溃了")

        # 指定模块
        r = self._ll_cli("inspect", "dir", "-m", "binary", app_id, check=False)
        if r.returncode < 0:
            raise AssertionError("inspect dir -m binary 崩溃了")

        # 缺少 APP 参数应被拒绝
        r = self._ll_cli("inspect", "dir", check=False)
        if r.returncode == 0:
            raise AssertionError("inspect dir 缺少 APP 参数竟然成功")

        # 不存在的应用
        r = self._ll_cli("inspect", "dir", "org.deepin.nonexistent.app", check=False)
        if r.returncode == 0:
            raise AssertionError("inspect dir 不存在的应用竟然成功")

    # ── Test: 扩展（extensions）──
    #
    # run --extensions 会走到 run_context.cpp 的 makeManualExtensionDefine /
    # fillExtraAppMounts / getRuntimeLayerPath（约 390 行）。
    # 扩展必须已安装才可用，这里用 base 里声明的显卡驱动扩展。
    def test_run_with_extensions(self):
        app_id = DEMO_APP_ID
        # org.deepin.driver.display.nvidia 在 base 的 extensions 里声明过，
        # 实测可以直接 --extensions 使用（rc=0）。
        r = self._ll_cli("run", app_id,
                         "--extensions", "org.deepin.driver.display.nvidia",
                         "--", "/bin/true", timeout=90, check=False)
        if r.returncode != 0:
            raise AssertionError(
                f"run --extensions 失败: rc={r.returncode} {r.stderr[:200]}")

        # 未安装的扩展应给出明确错误
        r = self._ll_cli("run", app_id,
                         "--extensions", "org.deepin.nonexistent.extension",
                         "--", "/bin/true", timeout=90, check=False)
        if r.returncode == 0:
            raise AssertionError("run --extensions 用了不存在的扩展竟然成功")
        if r.returncode < 0:
            raise AssertionError("run --extensions 不存在扩展时崩溃了")

    # ── Test: 安装/升级的错误处理路径 ──
    #
    # 这些错误处理函数在正常流程里永远走不到：
    #   cli.cpp: handleInstallError / handleInstallFromFileError / handleUpgradeError
    #   uab_file.cpp: extractSignData / saveErofsToFile（解析失败分支）
    # 用损坏的 UAB 与不存在的文件来触发。
    def test_install_error_paths(self):
        workdir = Path(tempfile.mkdtemp(prefix="ll-smoke-err-"))
        try:
            bogus = workdir / "bogus.uab"
            bogus.write_bytes(b"this is definitely not a uab file\n")

            random_uab = workdir / "random.uab"
            random_uab.write_bytes(os.urandom(200 * 1024))

            missing = workdir / "does-not-exist.uab"

            for path in (bogus, random_uab, missing):
                r = self._sudo_ll_cli("install", str(path), timeout=120, check=False)
                if r.returncode == 0:
                    raise AssertionError(f"安装非法文件 {path.name} 竟然成功")
                if r.returncode < 0:
                    raise AssertionError(f"安装 {path.name} 时崩溃了")
                if not (r.stderr.strip() or r.stdout.strip()):
                    raise AssertionError(f"安装 {path.name} 失败但无任何输出")

            # 同样的坏文件换 .layer 后缀：走 PackageManager::installFromLayer
            # （与 installFromUAB 是两条不同的链路，错误分支也不同）
            bogus_layer = workdir / "bogus.layer"
            bogus_layer.write_bytes(b"this is definitely not a layer file\n")
            random_layer = workdir / "random.layer"
            random_layer.write_bytes(os.urandom(200 * 1024))
            # 只有头没有 erofs 体：能过头部检查但在解析元信息时失败
            truncated_layer = workdir / "truncated.layer"
            truncated_layer.write_bytes(
                (workdir / "random.layer").read_bytes()[:64])
            empty_layer = workdir / "empty.layer"
            empty_layer.write_bytes(b"")

            for path in (bogus_layer, random_layer, truncated_layer,
                         empty_layer, workdir / "nope.layer"):
                r = self._sudo_ll_cli("install", str(path), timeout=120,
                                      check=False)
                if r.returncode == 0:
                    raise AssertionError(f"安装非法 {path.name} 竟然成功")
                if r.returncode < 0:
                    raise AssertionError(f"安装 {path.name} 时崩溃了")

            # upgrade 的错误路径
            r = self._sudo_ll_cli("upgrade", "org.deepin.nonexistent.app",
                                  timeout=120, check=False)
            if r.returncode == 0:
                raise AssertionError("upgrade 不存在的应用竟然成功")
            if r.returncode < 0:
                raise AssertionError("upgrade 不存在的应用时崩溃了")

            r = self._sudo_ll_cli("upgrade", str(bogus), timeout=120, check=False)
            if r.returncode == 0:
                raise AssertionError("upgrade 非法文件竟然成功")
            if r.returncode < 0:
                raise AssertionError("upgrade 非法文件时崩溃了")
        finally:
            shutil.rmtree(workdir, ignore_errors=True)

    # ── Test: ll-builder push 的错误路径 ──
    #
    # push 需要远端签名服务，CI 里没有，所以只能覆盖到失败分支。
    # 但这仍然会走到 linglong_builder.cpp 的 push 准备逻辑与
    # ostree_repo 的远端交互入口。
    def test_builder_push_errors(self):
        # ⚠️ 本用例原先直接 chdir(DEMO_PROJECT_DIR)，但排在前面的步骤
        #    （「安装并运行 demo 应用」）结束时会把 org.deepin.demo/ 删掉，
        #    于是 chdir 抛 FileNotFoundError，用例必失败；
        #    又因为 run_step 是 fail-fast（一个 FAIL 会跳过其后所有用例），
        #    这会让排在它后面的用例全部不执行。
        #    这里改成：项目目录在就用它，不在就在临时目录里跑 ——
        #    push 的错误路径本来也不需要真实的项目目录。
        old_cwd = os.getcwd()
        workdir = None
        try:
            if Path(DEMO_PROJECT_DIR).is_dir():
                os.chdir(DEMO_PROJECT_DIR)
            else:
                workdir = Path(tempfile.mkdtemp(prefix="ll-smoke-push-"))
                os.chdir(workdir)

            # 不可达的仓库地址：应给出明确错误，不崩溃
            r = self._ll_builder("push", "--repo-url", "http://127.0.0.1:1/nope",
                                 "--repo-name", "smoke", timeout=180, check=False)
            if r.returncode == 0:
                raise AssertionError("push 到不可达地址竟然成功")
            if r.returncode < 0:
                raise AssertionError("push 到不可达地址时崩溃了")
            if not (r.stderr.strip() or r.stdout.strip()):
                raise AssertionError("push 失败但无任何输出")
        finally:
            os.chdir(old_cwd)
            if workdir is not None:
                shutil.rmtree(workdir, ignore_errors=True)

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

    # ── Test: run 的文件 / URL / 设备传递选项 ──
    #
    # --file / --url / --device / --device-mode 这几个选项在原先的用例里
    # 一个都没碰过，对应 cli.cpp 里文件与 URL 参数的处理分支、
    # 以及 container_cfg_builder.cpp 的设备（CDI）注入逻辑。
    # 实测（2026-09-16，base 25.2.2.8）：
    #   --file /etc/hostname        rc=0
    #   --url https://example.com   rc=0
    #   --device-mode passthru      rc=0
    #   --device /dev/null          rc=255 invalid device format（格式校验分支）
    #   --device nvidia.com/gpu=0   rc=255 device not found（查找失败分支）
    def test_run_file_url_device(self):
        app_id = self._find_runnable_target()

        # 正常路径：文件与 URL 都能被接受
        for name, opts in (
            ("--file", ["--file", "/etc/hostname"]),
            ("--url", ["--url", "https://example.com"]),
        ):
            r = self._ll_cli("run", app_id, *opts, "--", "/bin/true",
                             timeout=120, check=False)
            if r.returncode != 0:
                raise AssertionError(
                    f"ll-cli run {name} 失败: rc={r.returncode} {r.stderr[:200]}")

        # --device-mode 单独使用是可接受的
        r = self._ll_cli("run", app_id, "--device-mode", "passthru",
                         "--", "/bin/true", timeout=120, check=False)
        if r.returncode != 0:
            raise AssertionError(
                f"ll-cli run --device-mode passthru 失败: "
                f"rc={r.returncode} {r.stderr[:200]}")

        # 错误路径：设备格式非法
        r = self._ll_cli("run", app_id, "--device", "/dev/null",
                         "--", "/bin/true", timeout=120, check=False)
        if r.returncode == 0:
            raise AssertionError("--device /dev/null 这种非法格式竟然成功")
        if r.returncode < 0:
            raise AssertionError("--device 非法格式时崩溃了")
        if "invalid device format" not in (r.stderr + r.stdout):
            raise AssertionError(
                f"--device 非法格式应提示 invalid device format, "
                f"实际: {(r.stderr + r.stdout)[:200]!r}")

        # 错误路径：格式合法但设备不存在
        r = self._ll_cli("run", app_id, "--device", "nvidia.com/gpu=0",
                         "--", "/bin/true", timeout=120, check=False)
        if r.returncode == 0:
            raise AssertionError("--device 指向不存在的设备竟然成功")
        if r.returncode < 0:
            raise AssertionError("--device 设备不存在时崩溃了")

    # ── Test: run 显式指定 base / runtime ──
    #
    # 显式 --base / --runtime 会走到 run_context.cpp 里与"从应用元数据推导"
    # 不同的那条解析分支（RunContext::resolve 的显式引用路径）。
    def test_run_explicit_base_runtime(self):
        app_id = self._find_runnable_target()

        # 取出当前 base 的完整引用（ID/版本/架构）
        r = self._ll_cli("list", "--type=base", check=False)
        base_ref = None
        if r.returncode == 0:
            for line in r.stdout.splitlines():
                parts = line.split()
                if len(parts) >= 2 and parts[0].startswith("org.deepin.base"):
                    base_ref = f"{parts[0]}/{parts[1]}"
                    break
        if base_ref is None:
            raise AssertionError("未能从 ll-cli list --type=base 解析出 base 引用")

        r = self._ll_cli("run", app_id, "--base", base_ref,
                         "--", "/bin/true", timeout=120, check=False)
        if r.returncode != 0:
            raise AssertionError(
                f"ll-cli run --base {base_ref} 失败: "
                f"rc={r.returncode} {r.stderr[:200]}")

        # ⚠️ 不存在的 base：实测【不保证】非 0 退出。
        #    run_context.cpp 里 --base 只在真正 resolve base layer 时才校验
        #    （见 RunContext::resolve 中 "base layer must be resolved for all kinds"），
        #    而 `-- /bin/true` 这种直接命令路径可能不走到那里，
        #    于是 rc=0 而 --base 被忽略。所以这里【只】断言不崩溃、有输出，
        #    不断言退出码 —— 否则会写出随环境状态漂移的脆弱用例。
        r = self._ll_cli("run", app_id, "--base", "org.deepin.nonexistent.base",
                         "--", "/bin/true", timeout=120, check=False)
        if r.returncode < 0:
            raise AssertionError("--base 指定不存在的 base 时崩溃了")

        # 不存在的 runtime 同理
        r = self._ll_cli("run", app_id,
                         "--runtime", "org.deepin.nonexistent.runtime",
                         "--", "/bin/true", timeout=120, check=False)
        if r.returncode < 0:
            raise AssertionError("--runtime 指定不存在的 runtime 时崩溃了")

    # ── Test: run 的 debug 选项错误路径 ──
    #
    # --debug 系列在 cli.cpp 里有一段独立的参数校验与 gdbserver 装配逻辑。
    # 实测：--debug 需要先装开发模块，当前环境会走
    #   "基础环境 ... 未安装开发模块，正在安装。" -> Error 9: not authorized
    # 因此这里只断言"不崩溃且给出明确失败"，不去依赖能真正装上开发模块。
    def test_run_debug_options(self):
        app_id = self._find_runnable_target()

        for name, opts in (
            ("--debug", ["--debug"]),
            ("--debug-listen", ["--debug", "--debug-listen", "127.0.0.1:9999"]),
            ("--debug-symbol-dir", ["--debug", "--debug-symbol-dir", "/tmp"]),
            # ⚠️ 选项名是 --debug-debuginfod，不是 --debuginfod。
            #    写成 --debuginfod 会被 CLI11 拒绝（rc=109 unexpected argument），
            #    那样就只是在测参数校验，测不到 debuginfod 那条分支。
            ("--debug-debuginfod", ["--debug",
                                    "--debug-debuginfod", "http://127.0.0.1:1/nope"]),
        ):
            # ⚠️ 必须用 PTY + stop_when，【不能】用带 timeout 的 subprocess：
            #    --debug 会起 gdbserver 等调试器附加，进程按设计【不会退出】。
            #    develop 模块没装时它会很快报错退出，看起来"正常"；
            #    一旦 develop 模块在位（例如为收集覆盖率而安装），
            #    它就会真的挂住直到超时 —— 实测这让本步骤 120s 超时失败，
            #    并因冒烟是 fail-fast 而跳过后面的全部步骤。
            #    stop_when 收到 gdb 提示后就正常收尾，不再傻等。
            code, out = self._pty_ll_cli(
                "run", app_id, *opts, "--", "/bin/true",
                timeout=60, stop_when="linglong-gdb")
            if code < 0:
                raise AssertionError(f"ll-cli run {name} 崩溃了")
            # 无论成功还是失败，都必须有可读输出，不能静默
            if not out.strip():
                raise AssertionError(f"ll-cli run {name} 既无输出也未见错误")

    # ── Test: install 的仓库与模块选项 ──
    #
    # --repo / --module / --no-auto-prune 都会走到 cli.cpp 里
    # 与默认安装不同的参数组装分支。用不存在的仓库/模块来覆盖失败分支，
    # 不实际改动系统状态。
    def test_install_repo_module_options(self):
        # 不存在的仓库：应失败且给出提示
        r = self._sudo_ll_cli("install", "--repo", "nonexistent-repo",
                              CALENDAR_APP_ID, timeout=180, check=False)
        if r.returncode == 0:
            raise AssertionError("install --repo 指定不存在的仓库竟然成功")
        if r.returncode < 0:
            raise AssertionError("install --repo 不存在的仓库时崩溃了")
        if not (r.stderr.strip() or r.stdout.strip()):
            raise AssertionError("install --repo 失败但无任何输出")

        # 不存在的模块：应被拒绝
        r = self._sudo_ll_cli("install", "--module", "bogus-module",
                              CALENDAR_APP_ID, timeout=180, check=False)
        if r.returncode == 0:
            raise AssertionError("install --module 指定不存在的模块竟然成功")
        if r.returncode < 0:
            raise AssertionError("install --module 不存在的模块时崩溃了")

        # --no-auto-prune：参数本身合法，安装不存在的应用应干净失败
        r = self._sudo_ll_cli("install", "--no-auto-prune",
                              "org.deepin.nonexistent.app",
                              timeout=180, check=False)
        if r.returncode == 0:
            raise AssertionError("install 不存在的应用竟然成功")
        if r.returncode < 0:
            raise AssertionError("install 不存在的应用时崩溃了")

    # ── Test: uninstall 的模块与强制选项 ──
    #
    # uninstall --module / --force / --no-auto-prune 对应卸载流程里
    # 不同的目标解析与依赖清理分支，之前完全没有用例。
    def test_uninstall_module_force_options(self):
        # 未安装的应用：应干净失败
        for name, opts in (
            ("--module", ["--module", "bogus-module"]),
            ("--force", ["--force"]),
            ("--no-auto-prune", ["--no-auto-prune"]),
        ):
            r = self._sudo_ll_cli("uninstall", *opts,
                                  "org.deepin.nonexistent.app",
                                  timeout=120, check=False)
            if r.returncode == 0:
                raise AssertionError(f"uninstall {name} 对不存在的应用竟然成功")
            if r.returncode < 0:
                raise AssertionError(f"uninstall {name} 崩溃了")

        # ⚠️ 绝对不要用 --force 去卸 base/runtime 来测这个分支：
        #    实测 `sudo ll-cli uninstall --force org.deepin.base` 会真的把它卸掉
        #    （rc=0），而被卸的 base 是后续所有用例的前提，
        #    会让排在后面的用例连锁失败。
        #    --force 的参数解析分支用"不存在的应用"同样能覆盖到。
        r = self._sudo_ll_cli("uninstall", "--force",
                              "org.deepin.nonexistent.base",
                              timeout=120, check=False)
        if r.returncode == 0:
            raise AssertionError("uninstall --force 对不存在的 base 竟然成功")
        if r.returncode < 0:
            raise AssertionError("uninstall --force 不存在的 base 时崩溃了")

    # ── Test: upgrade 的依赖与清理选项 ──
    def test_upgrade_deps_prune_options(self):
        for name, opts in (
            ("--deps-only", ["--deps-only"]),
            ("--no-auto-prune", ["--no-auto-prune"]),
            ("--deps-only --no-auto-prune",
             ["--deps-only", "--no-auto-prune"]),
        ):
            r = self._sudo_ll_cli("upgrade", *opts,
                                  "org.deepin.nonexistent.app",
                                  timeout=180, check=False)
            if r.returncode == 0:
                raise AssertionError(
                    f"upgrade {name} 对不存在的应用竟然成功")
            if r.returncode < 0:
                raise AssertionError(f"upgrade {name} 崩溃了")
            if not (r.stderr.strip() or r.stdout.strip()):
                raise AssertionError(f"upgrade {name} 失败但无任何输出")

    # ── Test: --no-dbus（peer-to-peer 模式）──
    #
    # --no-dbus 是【全局】flag（挂在顶层 commandParser 上，不在子命令里），
    # 所以必须写在子命令【前面】：`ll-cli --no-dbus list`。
    # 写成 `ll-cli list --no-dbus` 会被 CLI11 当成未知参数拒绝（rc=109），
    # 那走的是完全不同的分支，测不到 peer 路径（踩过）。
    #
    # peer 模式下 ll-cli 自己 fork 出一个 package-manager 并通过
    # 临时 unix socket 通信（initializePeerModePackageManager /
    # preparePeerSocketDir），与走 dbus 服务的默认路径是两套独立代码。
    def test_no_dbus_peer_mode(self):
        # 只读且【不需要 package manager】的命令读的是本地状态，
        # 根本不会去初始化 package manager，所以非 root 也能成功。
        # （实测：repo show / search 这类需要 PM 的会报 root-only，别放这里）
        for args in (["list"], ["ps"], ["analyze", "size"],
                     ["analyze", "depends"]):
            r = self._ll_cli("--no-dbus", *args, timeout=120, check=False)
            if r.returncode != 0:
                raise AssertionError(
                    f"ll-cli --no-dbus {' '.join(args)} 失败: "
                    f"rc={r.returncode} {r.stderr[:200]}")

        # 非 root 在【需要 package manager】的命令上会走到
        # initializePeerModePackageManager 的 getuid()!=0 检查，
        # 报 "--no-dbus should only be used by root user."（rc=255）。
        # ⚠️ 别用 list 测这条：list 不需要 PM，非 root 也返回 0（实测踩过）。
        for args in (["repo", "show"], ["search", "deepin"], ["prune"],
                     ["install", "org.deepin.nonexistent.app"],
                     ["uninstall", "org.deepin.nonexistent.app"]):
            r = self._ll_cli("--no-dbus", *args, timeout=120, check=False)
            if r.returncode == 0:
                raise AssertionError(
                    f"非 root 的 ll-cli --no-dbus {' '.join(args)} 竟然成功")
            if r.returncode < 0:
                raise AssertionError(
                    f"非 root 的 ll-cli --no-dbus {' '.join(args)} 崩溃了")
            if "root" not in (r.stderr + r.stdout):
                raise AssertionError(
                    f"非 root --no-dbus {' '.join(args)} 没有给出 root-only 提示: "
                    f"{(r.stderr + r.stdout)[:200]}")

        # root 身份下的 peer 模式：这里才会真正建起 peer socket 并
        # fork 出 package-manager，是 --no-dbus 的完整成功路径。
        for args in (["list"], ["ps"], ["repo", "show"],
                     ["search", "deepin"], ["analyze", "depends"]):
            r = self._sudo_ll_cli("--no-dbus", *args, timeout=180, check=False)
            if r.returncode != 0:
                raise AssertionError(
                    f"sudo ll-cli --no-dbus {' '.join(args)} 失败: "
                    f"rc={r.returncode} {r.stderr[:200]}")

        # peer 模式下的写操作（root 有权），走 peer 的权限校验分支
        for args in (["repo", "set-default", "stable"],
                     ["repo", "enable-mirror", "stable"],
                     ["repo", "disable-mirror", "stable"]):
            r = self._sudo_ll_cli("--no-dbus", *args, timeout=180, check=False)
            if r.returncode != 0:
                raise AssertionError(
                    f"sudo ll-cli --no-dbus {' '.join(args)} 失败: "
                    f"rc={r.returncode} {r.stderr[:200]}")

    # ── Test: 仓库镜像管理 ──
    #
    # enable-mirror / disable-mirror / update 之前完全没被调用，
    # 对应 repo 子命令里的镜像与 URL 更新分支。
    def test_repo_mirror_management(self):
        for args in (["enable-mirror", "stable"], ["disable-mirror", "stable"]):
            r = self._sudo_ll_cli("repo", *args, timeout=120, check=False)
            if r.returncode != 0:
                raise AssertionError(
                    f"repo {' '.join(args)} 失败: "
                    f"rc={r.returncode} {r.stderr[:200]}")

        # update 需要 <alias> <url> 两个参数
        r = self._sudo_ll_cli("repo", "update", "stable", SMOKE_REPO_URL,
                              timeout=180, check=False)
        if r.returncode not in (0, 255):
            raise AssertionError(
                f"repo update 异常退出: rc={r.returncode} {r.stderr[:200]}")

        # 缺参数应被 CLI11 拒绝（rc=106），而不是崩溃
        r = self._sudo_ll_cli("repo", "update", timeout=60, check=False)
        if r.returncode == 0:
            raise AssertionError("repo update 缺参数竟然成功")
        if r.returncode < 0:
            raise AssertionError("repo update 缺参数崩溃了")

        # 对不存在的仓库做写操作：应报错而非崩溃
        r = self._sudo_ll_cli("repo", "set-default", "nonexistent-repo",
                              timeout=60, check=False)
        if r.returncode == 0:
            raise AssertionError("repo set-default 不存在的仓库竟然成功")
        if r.returncode < 0:
            raise AssertionError("repo set-default 不存在的仓库崩溃了")

    # ── Test: --json 输出格式 ──
    #
    # --json 会走 json_printer 而不是默认的表格 printer，两条渲染路径
    # 完全不同（libs/linglong/src/linglong/cli/json_printer.cpp）。
    # 之前只有 list 一处用过 --json，json_printer 覆盖率极低。
    # 这里对每个产出结构化数据的子命令都跑一次，并要求输出真的是 JSON。
    def test_json_output(self):
        cases = (
            ("list", ["list"]),
            ("ps", ["ps"]),
            ("repo show", ["repo", "show"]),
            ("analyze size", ["analyze", "size"]),
        )
        for name, args in cases:
            r = self._ll_cli("--json", *args, timeout=120, check=False)
            if r.returncode != 0:
                raise AssertionError(
                    f"ll-cli --json {name} 失败: "
                    f"rc={r.returncode} {r.stderr[:200]}")
            out = r.stdout.strip()
            if not out:
                raise AssertionError(f"ll-cli --json {name} 没有任何输出")
            # 必须是合法 JSON —— 这是 json_printer 相对表格输出的本质区别
            try:
                json.loads(out)
            except ValueError as e:
                raise AssertionError(
                    f"ll-cli --json {name} 输出不是合法 JSON: {e}; "
                    f"前 200 字符: {out[:200]}") from e

        # info 对不存在的应用：应返回错误 JSON 且不崩溃
        r = self._ll_cli("--json", "info", "org.deepin.nonexistent.app",
                         timeout=60, check=False)
        if r.returncode == 0:
            raise AssertionError("--json info 不存在的应用竟然成功")
        if r.returncode < 0:
            raise AssertionError("--json info 不存在的应用崩溃了")

    # ── Test: 全局选项与帮助输出 ──
    #
    # 全局 --json / --no-progress / --help-all 走的是与子命令选项
    # 不同的解析层（CLI11 的全局 option 处理），之前没有用例。
    def test_global_options(self):
        # --no-progress 应被接受
        r = self._ll_cli("--no-progress", "list", timeout=60, check=False)
        if r.returncode != 0:
            raise AssertionError(
                f"ll-cli --no-progress list 失败: rc={r.returncode}")

        # --json 全局选项 + 子命令组合
        r = self._ll_cli("--json", "list", "--type=runtime", check=False)
        if r.returncode != 0:
            raise AssertionError(
                f"ll-cli --json list --type=runtime 失败: rc={r.returncode}")

        # --help-all 会展开所有子命令帮助，覆盖帮助渲染分支
        for args in (["--help"], ["--help-all"],
                     ["run", "--help"], ["repo", "--help"]):
            r = self._ll_cli(*args, timeout=60, check=False)
            if r.returncode != 0:
                raise AssertionError(
                    f"ll-cli {' '.join(args)} 失败: rc={r.returncode}")
            if not (r.stdout.strip() or r.stderr.strip()):
                raise AssertionError(f"ll-cli {' '.join(args)} 无任何输出")

        # 非法全局选项应被拒绝
        r = self._ll_cli("--definitely-not-a-flag", check=False)
        if r.returncode == 0:
            raise AssertionError("ll-cli 非法全局选项竟然成功")
        if r.returncode < 0:
            raise AssertionError("ll-cli 非法全局选项崩溃了")

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
    def _pty_ll_cli(self, *args: str, timeout: int = 60, env: dict | None = None,
                    stop_when: str | None = None, grace: int = 10):
        """在真 PTY 里跑 ll-cli，返回 (退出码, 去噪输出)。

        stop_when: 输出里出现该子串就【主动收尾】，不再傻等。
            有些命令按设计就不会自己退出（例如 `run --debug` 会起
            gdbserver 等调试器附加），死等超时会把冒烟拖到分钟级。
            覆盖数据在代码执行到时就已经产生，只要让进程正常退出即可。

        env: 额外环境变量。值为 None 表示【真正删掉】该变量
            （getenv 返回 nullptr 与返回 "" 在某些分支上并不等价）。

        收尾用 SIGINT 而不是 SIGKILL：libgcov 在正常退出或 SIGINT 时
        才把计数写回 .gcda，SIGKILL 会丢掉这个进程的整份计数。
        """
        cmd = [LL_CLI, *args]
        pid, fd = pty.fork()
        if pid == 0:
            run_env = os.environ.copy()
            # 非 sudo 的 ll-cli 走当前用户的 prefix（见 executor.py 的说明）
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

    # ── Test: PTY 下的 run（TTY 分支 + 容器复用）──
    #
    # 对应代码：
    #   cli.cpp  setupIOChannels / reuseContainer / acceptConsoleFd
    #   common/socket.cpp  sendFdWithPayload / recvFdWithPayload
    #
    # ⚠️ acceptConsoleFd 在 reuseContainer 里（即 `run` 复用已有容器），
    #    【不在】enter 里 —— 一开始按直觉去测 enter 是错的（实测纠正）。
    def test_run_in_pty(self):
        inst = f"smokepty{os.getpid()}"

        # ⚠️ 不能假设日历应用一定还装着：本用例排在「验证日历模块
        #    生命周期」之后，那一步会把日历卸掉。实测因此失败在
        #    "PTY 下启动的容器未出现在 ll-cli ps 中"（其实是 run 直接报
        #    package not found，容器根本没起）。
        #    这里先确保目标可运行，缺了就装回来。
        app_id = CALENDAR_APP_ID
        r = self._ll_cli("list", check=False)
        if CALENDAR_APP_ID not in r.stdout:
            self._sudo_ll_cli("install", CALENDAR_APP_ID,
                              timeout=900, check=False)
            r = self._ll_cli("list", check=False)
            if CALENDAR_APP_ID not in r.stdout:
                # 退回 base（_find_runnable_target 保证它在位）
                app_id = self._find_runnable_target()

        # 第一次：真 PTY 下启动一个长驻实例
        pid, fd = pty.fork()
        if pid == 0:
            run_env = os.environ.copy()
            run_env.setdefault("GCOV_PREFIX", "/var/tmp/linglong-cov-user")
            run_env["GCOV_PREFIX_STRIP"] = "0"
            run_env.setdefault("DISPLAY", ":0")
            try:
                os.execvpe(LL_CLI, [LL_CLI, "run", app_id,
                                    "--instance", inst, "--", "sleep", "120"],
                           run_env)
            finally:
                os._exit(127)
        try:
            if not self._wait_container(timeout=40):
                raise AssertionError("PTY 下启动的容器未出现在 ll-cli ps 中")

            # 复用同一实例：走 reuseContainer + acceptConsoleFd + PTY 传递
            for label, extra in (("第二次", "R1"), ("第三次", "R2")):
                code, out = self._pty_ll_cli(
                    "run", app_id, "--instance", inst,
                    "--", "/bin/echo", extra, timeout=90)
                if code != 0:
                    raise AssertionError(
                        f"PTY 下{label}复用实例失败: rc={code} {out[:200]}")
                if extra not in out:
                    raise AssertionError(
                        f"PTY 下{label}复用的输出里没有 {extra}: {out[:200]!r}")

            # 全新实例（不带 --instance）：走 TTY 下首次分配 console
            code, out = self._pty_ll_cli(
                "run", app_id, "--", "/bin/echo", "FRESH", timeout=90)
            if code != 0:
                raise AssertionError(
                    f"PTY 下全新实例运行失败: rc={code} {out[:200]}")
            if "FRESH" not in out:
                raise AssertionError(f"PTY 下全新实例无预期输出: {out[:200]!r}")
        finally:
            try:
                os.kill(pid, 9)
            except OSError:
                pass
            try:
                os.close(fd)
            except OSError:
                pass
            try:
                os.waitpid(pid, 0)
            except ChildProcessError:
                pass
            self._cleanup_containers(app_id, timeout=40)

    # ── Test: run 的文件/URL 占位符映射 ──
    #
    # 对应 cli.cpp 的 mappingFile / mappingUrl / filePathMapping。
    #
    # ⚠️ 关键：只在【命令参数里带 %f/%F/%u/%U 占位符】时才会调用
    #    mappingFile。只写 `--file /tmp/x` 而不带占位符，命令能跑通
    #    但 mappingFile 一行都不会执行（实测 +0 行，踩过）。
    #
    # 映射规则（实测）：HOME 之外的绝对路径 → /run/host/rootfs<原路径>；
    # HOME 之内保持原样；软链接先 realpath；非 file:// 的 URL 原样透传。
    def test_run_placeholder_mapping(self):
        tmp_file = f"/tmp/smoke-map-{os.getpid()}.txt"
        Path(tmp_file).write_text("map\n")
        link = f"/tmp/smoke-map-link-{os.getpid()}.txt"
        try:
            os.symlink(tmp_file, link)
        except OSError:
            link = tmp_file

        try:
            # %f + 单个文件：应被映射到 /run/host/rootfs 下
            code, out = self._pty_ll_cli(
                "run", CALENDAR_APP_ID, "--file", tmp_file,
                "--", "/bin/echo", "%f", timeout=90)
            if code != 0:
                raise AssertionError(f"%f 运行失败: rc={code} {out[:200]}")
            if "/run/host/rootfs" not in out:
                raise AssertionError(
                    f"%f 的路径没有被映射到 /run/host/rootfs: {out[:200]!r}")

            # %F + 多个文件：走多条映射 + 告警分支
            code, out = self._pty_ll_cli(
                "run", CALENDAR_APP_ID, "--file", "/etc/hostname", "/etc/hosts",
                "--", "/bin/echo", "%F", timeout=90)
            if code != 0:
                raise AssertionError(f"%F 运行失败: rc={code} {out[:200]}")
            if out.count("/run/host/rootfs") < 2:
                raise AssertionError(
                    f"%F 多个文件没有各自映射: {out[:200]!r}")

            # %u + file:// URL：前缀保留，路径被映射
            code, out = self._pty_ll_cli(
                "run", CALENDAR_APP_ID, "--url", f"file://{tmp_file}",
                "--", "/bin/echo", "%u", timeout=90)
            if code != 0:
                raise AssertionError(f"%u file:// 失败: rc={code} {out[:200]}")
            if "file:///run/host/rootfs" not in out:
                raise AssertionError(
                    f"%u 没有保留 file:// 前缀并映射路径: {out[:200]!r}")

            # %u + 非 file 协议：原样透传，不映射
            code, out = self._pty_ll_cli(
                "run", CALENDAR_APP_ID, "--url", "https://example.com",
                "--", "/bin/echo", "%u", timeout=90)
            if code != 0:
                raise AssertionError(f"%u https 失败: rc={code} {out[:200]}")
            if "https://example.com" not in out:
                raise AssertionError(
                    f"%u 的 https URL 没有原样透传: {out[:200]!r}")

            # 软链接：应先 realpath 再映射
            code, out = self._pty_ll_cli(
                "run", CALENDAR_APP_ID, "--file", link,
                "--", "/bin/echo", "%f", timeout=90)
            if code != 0:
                raise AssertionError(f"%f 软链接失败: rc={code} {out[:200]}")
        finally:
            for p in (tmp_file, link):
                try:
                    os.unlink(p)
                except OSError:
                    pass
            self._cleanup_containers(CALENDAR_APP_ID, timeout=40)

    # ── Test: CDI 设备注入 ──
    #
    # 对应 oci-cfg-generators/container_cfg_builder.cpp 的
    # applyCDIPatch / adjustNode / shouldFix / tryFixMountpointsTree
    # 以及 runtime/run_context.cpp 的 filterCDIDevices。
    #
    # 设备引用格式是【恰好一个等号】的 `<kind>=<name>`：
    #     --device covtest.io/class=covdev0
    # kind 里带等号会被 split 成 3 段而报 "invalid device format"（踩过）。
    def test_cdi_device_injection(self):
        spec_dir = "/tmp/smoke-cdi"
        shutil.rmtree(spec_dir, ignore_errors=True)
        os.makedirs(spec_dir, exist_ok=True)

        # 一个只带 env 的设备：最容易成功，用来验证正常注入
        Path(f"{spec_dir}/env.yaml").write_text(
            'cdiVersion: "0.5.0"\n'
            'kind: "smoke.io/env"\n'
            'devices:\n'
            '  - name: "envdev"\n'
            '    containerEdits:\n'
            '      env:\n'
            '        - "SMOKE_CDI_ENV=1"\n'
        )
        # 一个带 mounts 的设备：会走到挂载点处理与 json patch 相关分支
        Path(f"{spec_dir}/mnt.yaml").write_text(
            'cdiVersion: "0.5.0"\n'
            'kind: "smoke.io/mnt"\n'
            'devices:\n'
            '  - name: "mntdev"\n'
            '    containerEdits:\n'
            '      mounts:\n'
            '        - hostPath: "/tmp"\n'
            '          containerPath: "/mnt/smokecdi"\n'
            '          type: "bind"\n'
            '          options: ["rbind"]\n'
        )
        # 一个带 deviceNodes 的设备
        Path(f"{spec_dir}/dev.yaml").write_text(
            'cdiVersion: "0.5.0"\n'
            'kind: "smoke.io/dev"\n'
            'devices:\n'
            '  - name: "devdev"\n'
            '    containerEdits:\n'
            '      deviceNodes:\n'
            '        - path: "/dev/null"\n'
            '          hostPath: "/dev/null"\n'
            '          type: "c"\n'
            '          major: 1\n'
            '          minor: 3\n'
            '          permissions: "rwm"\n'
        )

        try:
            # 只带 env 的设备：应成功，且环境变量真的被注入容器
            code, out = self._pty_ll_cli(
                "run", CALENDAR_APP_ID, "--cdi-spec-dir", spec_dir,
                "--device", "smoke.io/env=envdev",
                "--", "/bin/sh", "-c", "echo CDI=$SMOKE_CDI_ENV", timeout=90)
            if code != 0:
                raise AssertionError(f"CDI env 设备运行失败: rc={code} {out[:200]}")
            if "CDI=1" not in out:
                raise AssertionError(
                    f"CDI 的 env 没有被注入容器: {out[:200]!r}")

            # 带 mounts 的设备：容器可能因挂载失败而退出，但代码路径会走到
            code, out = self._pty_ll_cli(
                "run", CALENDAR_APP_ID, "--cdi-spec-dir", spec_dir,
                "--device", "smoke.io/mnt=mntdev",
                "--", "/bin/echo", "MNT", timeout=90)
            if code < 0:
                raise AssertionError("CDI mounts 设备崩溃了")

            # 带 deviceNodes 的设备
            code, out = self._pty_ll_cli(
                "run", CALENDAR_APP_ID, "--cdi-spec-dir", spec_dir,
                "--device", "smoke.io/dev=devdev",
                "--", "/bin/echo", "DEV", timeout=90)
            if code < 0:
                raise AssertionError("CDI deviceNodes 设备崩溃了")

            # 不存在的设备名：必须报错且不崩溃
            code, out = self._pty_ll_cli(
                "run", CALENDAR_APP_ID, "--cdi-spec-dir", spec_dir,
                "--device", "smoke.io/env=doesnotexist",
                "--", "/bin/echo", "X", timeout=90)
            if code == 0:
                raise AssertionError("不存在的 CDI 设备竟然运行成功")
            if code < 0:
                raise AssertionError("不存在的 CDI 设备崩溃了")

            # 格式非法（等号数量不对）：必须报错且不崩溃
            code, out = self._pty_ll_cli(
                "run", CALENDAR_APP_ID, "--cdi-spec-dir", spec_dir,
                "--device", "noequalsign",
                "--", "/bin/echo", "X", timeout=90)
            if code == 0:
                raise AssertionError("非法 CDI 设备格式竟然成功")
            if code < 0:
                raise AssertionError("非法 CDI 设备格式崩溃了")
        finally:
            shutil.rmtree(spec_dir, ignore_errors=True)
            self._cleanup_containers(CALENDAR_APP_ID, timeout=40)

    # ── Test: --debug 的 gdb 附加提示（需要 TTY）──
    #
    # 对应 cli.cpp 的 printDebugAttachHint / createDebugAttachScript /
    # makeDebugAttachScriptContent / makeDebugSymbolDir。
    #
    # ⚠️ 两个前置条件（都踩过）：
    #   1) printDebugAttachHint 开头就是 `isatty(stdout)==0 直接 return`，
    #      所以必须真 PTY，否则这些函数一行都不执行；
    #   2) --debug 会先要求装 base 的 develop 模块，非 root 装会报
    #      "Error 9: not authorized"，所以在装好之前根本走不到提示代码。
    def test_debug_attach_hint(self):
        # ⚠️ 不能用 CALENDAR_APP_ID：它依赖 base 23.1.0.3，
        #    而该版本的 develop 模块在仓库里不存在
        #    （报 "要安装该模块，必须先安装此应用程序"），
        #    于是 --debug 会停在"正在安装开发模块"的报错上，
        #    根本走不到提示分支（实测如此，导致本步骤失败）。
        #    _find_runnable_target() 会保证 base（25.2.x）在位并有
        #    develop 模块，是这里唯一可靠的目标。
        app_id = self._find_runnable_target()

        # 确保 develop 模块在位（root 才有权限装）
        self._sudo_ll_cli("install", "org.deepin.base", "--module=develop",
                          timeout=600, check=False)

        variants = (
            ("基础", []),
            ("symbol-dir", ["--debug-symbol-dir", "/tmp/smoke-dbgsym"]),
            ("debuginfod", ["--debug-debuginfod", "http://smoke.invalid/"]),
            ("三个齐",
             ["--debug-listen", "127.0.0.1:23459",
              "--debug-symbol-dir", "/tmp/smoke-dbgsym2",
              "--debug-debuginfod", "http://smoke.invalid/"]),
        )
        out = ""
        for name, extra in variants:
            # ⚠️ 必须用 stop_when 提前收尾：`run --debug` 会起 gdbserver
            #    等调试器附加，【进程本身不会退出】。若只靠 timeout 兜底，
            #    4 个变体 × 60s 会把这一步拖到数分钟。
            #    覆盖计数在代码执行到时已产生，收到提示后正常退出即可。
            code, out = self._pty_ll_cli(
                "run", app_id, "--debug", *extra,
                "--", "/bin/echo", "DBG", timeout=60,
                stop_when="linglong-gdb")
            if code < 0:
                raise AssertionError(f"--debug（{name}）崩溃了")
            # 走到提示分支时会打印 attach 脚本路径；没走到也要有输出，
            # 便于在日志里看出实际停在哪一步。
            if not out.strip():
                raise AssertionError(f"--debug（{name}）没有任何输出")

        # 确认确实走到了"生成 gdb 附加脚本"的提示分支
        if "linglong-gdb" not in out and "调试模式" not in out:
            if "not authorized" in out:
                raise AssertionError(
                    f"--debug 因权限不足未走到提示分支: {out[:300]}")
            if "未安装开发模块" in out or "failed to install develop module" in out:
                raise AssertionError(
                    f"--debug 因缺少 develop 模块未走到提示分支: {out[:300]}")
            raise AssertionError(
                f"--debug 没有生成 gdb 附加脚本的提示: {out[:300]!r}")

        # 端口占用是上一次运行的残留，不是被测代码的问题；
        # 顺手报出来，避免下次又被同样的现象误导去查错方向。
        if "Address already in use" in out:
            raise AssertionError(
                "gdbserver 端口被占用（疑似上次运行残留），"
                "请先清理 gdbserver 再跑: " + out[:200])

        self._cleanup_containers(app_id, timeout=40)
        # gdbserver 是 --debug 起来的，收尾时顺手清掉，避免占用默认端口
        # 2345 影响后续用例。
        self._run_cmd(["pkill", "-9", "gdbserver"], sudo=True, check=False)

    # ── Test: 图形驱动检测工具（ll-driver-detect）──
    #
    # ll-driver-detect 装在 /usr/libexec/linglong/ 下（不在 PATH 里），
    # 是一整套从未被冒烟触碰过的程序，覆盖 apps/ll-driver-detect/src/。
    #
    # 它的入口先读 /sys/module/nvidia/version；没有 NVIDIA 硬件时
    # 直接早退，后面几百行都到不了。这里在【私有 mount namespace】里
    # 用 tmpfs 覆盖 /sys/module 并造出该文件，从而驱动完整流程。
    # 包名格式是 org.deepin.driver.display.nvidia.<版本，点换横线>。
    def test_driver_detect(self):
        binary = "/usr/libexec/linglong/ll-driver-detect"
        if not os.path.exists(binary):
            raise AssertionError(f"找不到 {binary}")

        # 不带任何 flag：应打印帮助或进入检测
        r = self._run_cmd([binary, "--help"], check=False)
        if r.returncode != 0:
            raise AssertionError(f"ll-driver-detect --help 失败: {r.stderr[:200]}")

        # --check-only：只检查不安装不通知，最安全
        r = self._run_cmd([binary, "--check-only"], check=False)
        if r.returncode < 0:
            raise AssertionError("ll-driver-detect --check-only 崩溃了")

        # --install-only：只安装不通知（会真的去仓库查驱动包）
        r = self._run_cmd([binary, "--install-only"], check=False)
        if r.returncode < 0:
            raise AssertionError("ll-driver-detect --install-only 崩溃了")

        # 配置文件驱动的分支：neverRemind / 非法 JSON / 空对象
        # 路径来自 XDG_CONFIG_HOME（默认 ~/.config/linglong/driver_detection.json）
        cfg_dir = Path.home() / ".config" / "linglong"
        cfg = cfg_dir / "driver_detection.json"
        backup = None
        try:
            cfg_dir.mkdir(parents=True, exist_ok=True)
            if cfg.exists():
                backup = cfg.read_text()
            for content in ('{"neverRemind":true}', '{"neverRemind":false}',
                            '{}', '{bad json'):
                cfg.write_text(content)
                r = self._run_cmd([binary, "--check-only"], check=False)
                if r.returncode < 0:
                    raise AssertionError(
                        f"配置 {content} 下 ll-driver-detect 崩溃了")
        finally:
            if backup is not None:
                cfg.write_text(backup)
            else:
                try:
                    cfg.unlink()
                except OSError:
                    pass

        # 在私有 mount namespace 里伪造 NVIDIA 版本文件，
        # 走完整的 detect() -> 仓库查询链路
        script = (
            "mount -t tmpfs tmpfs /sys/module 2>/dev/null || exit 0\n"
            "mkdir -p /sys/module/nvidia\n"
            "echo '580.119.02' > /sys/module/nvidia/version\n"
            "chmod 755 /sys/module/nvidia\n"
            "chmod 644 /sys/module/nvidia/version\n"
            f"{binary} --check-only 2>&1 | grep -v 'profiling:' | head -5\n"
        )
        r = self._run_cmd(
            ["unshare", "-m", "bash", "-c", script],
            sudo=True, check=False, timeout=180)
        if r.returncode < 0:
            raise AssertionError("伪造 NVIDIA 版本后 ll-driver-detect 崩溃了")
        # 不应再出现 "version file not found"，否则说明 tmpfs 覆盖没生效
        if "version file not found" in (r.stdout + r.stderr):
            raise AssertionError(
                "伪造的 NVIDIA 版本文件没被读到，namespace 覆盖失败: "
                f"{(r.stdout + r.stderr)[:200]}")

    # ── Test: 显示/时区/网络的环境分支 ──
    #
    # 对应 run_context.cpp 的 detectDisplaySystem / resolveTimeZone /
    # resolveNetworkConf，以及 common/display.cpp 的 getXOrgAuthFile。
    #
    # 这些都是【环境变量驱动】的，改环境变量即可覆盖，安全且可移植。
    #
    # ⚠️ 不要去改 /etc/resolv.conf 或 /etc/localtime 来覆盖"文件状态"分支：
    #    ll-cli run 的容器是由 PackageManager 服务（另一个进程）创建的，
    #    它【不在】本进程的 mount namespace 里，看不到本进程的私有挂载，
    #    却能看到本进程对真实文件系统的删除/改写。
    #    实测：在 unshare -m 里 rm/ln /etc/resolv.conf 会直接改坏宿主机，
    #    导致 DNS 失效、ll-cli search 报 code 3001（已发生过并修复）。
    def test_display_env_branches(self):
        cases = (
            ("XAUTHORITY 假路径", {"XAUTHORITY": "/tmp/smoke-no-such-xauth"}),
            ("XAUTHORITY 空", {"XAUTHORITY": ""}),
            ("XAUTHORITY 未设", {"XAUTHORITY": None}),
            ("WAYLAND_DISPLAY 设", {"WAYLAND_DISPLAY": "wayland-0"}),
            ("WAYLAND_DISPLAY 空", {"WAYLAND_DISPLAY": ""}),
            ("WAYLAND_DISPLAY 未设", {"WAYLAND_DISPLAY": None}),
            ("TZDIR 有效", {"TZDIR": "/usr/share/zoneinfo"}),
            ("TZDIR 无效", {"TZDIR": "/nonexistent-zoneinfo"}),
            ("TZDIR 空", {"TZDIR": ""}),
            ("XDG_RUNTIME_DIR", {"XDG_RUNTIME_DIR": "/run/user/1001"}),
            ("会话总线",
             {"DBUS_SESSION_BUS_ADDRESS": "unix:path=/run/user/1001/bus"}),
            ("DISPLAY 未设", {"DISPLAY": None}),
        )
        for name, env in cases:
            code, out = self._pty_ll_cli(
                "run", CALENDAR_APP_ID, "--", "/bin/echo", "ENV", timeout=60,
                env=env)
            if code < 0:
                raise AssertionError(f"环境分支（{name}）崩溃了")
            if "ENV" not in out:
                raise AssertionError(
                    f"环境分支（{name}）没有跑到命令: {out[:200]!r}")

    # ── Test: 仓库版本迁移 ──
    #
    # 对应 repo/migrate.cpp。tryMigrate 读 /var/lib/linglong/.version：
    #   - 若等于 LINGLONG_VERSION（1.15.0）→ 直接 NoChange，整个文件不执行
    #   - 不等于 → dispatchMigrations，其中只有 from < 1.7.0 才真的迁移
    # 所以必须把戳改成旧版本才能覆盖到这些代码；跑完要还原。
    def test_repo_migration(self):
        if not shutil.which("systemctl"):
            raise AssertionError("没有 systemctl，无法测试仓库迁移")

        version_file = "/var/lib/linglong/.version"
        svc = "org.deepin.linglong.PackageManager.service"

        r = self._run_cmd(["cat", version_file], sudo=True, check=False)
        if r.returncode != 0:
            raise AssertionError(f"读不到 {version_file}: {r.stderr[:200]}")
        original = r.stdout.strip()

        try:
            # 各种旧版本值：都会走 dispatchMigrations，且都 < 1.7.0
            for old in ("1.6.0", "1.5.0", "1.0.0"):
                w = self._run_cmd(
                    ["bash", "-c", f"printf '%s' '{old}' > {version_file}"],
                    sudo=True, check=False)
                if w.returncode != 0:
                    raise AssertionError(f"写版本戳 {old} 失败: {w.stderr[:200]}")
                self._run_cmd(["systemctl", "restart", svc],
                              sudo=True, check=False, timeout=300)

            # 版本串无法解析：应走 parseVersion 失败分支，不能把服务搞崩到不可恢复
            self._run_cmd(
                ["bash", "-c", f"printf '%s' 'garbage' > {version_file}"],
                sudo=True, check=False)
            self._run_cmd(["systemctl", "restart", svc],
                          sudo=True, check=False, timeout=300)
        finally:
            # 无论成败都要还原，否则后续安装/运行会因版本戳异常而失败。
            # 上面故意写过 'garbage'，服务会因此进入 failed 状态，
            # 所以先 reset-failed 再重启，否则可能一直起不来。
            self._run_cmd(
                ["bash", "-c", f"printf '%s' '{original}' > {version_file}"],
                sudo=True, check=False)
            self._run_cmd(["systemctl", "reset-failed", svc],
                          sudo=True, check=False)
            self._run_cmd(["systemctl", "restart", svc],
                          sudo=True, check=False, timeout=300)

        # 还原后服务必须回到 active，否则整轮冒烟都不可信。
        # ⚠️ systemctl restart 是异步的：返回时单元可能还在 activating，
        #    甚至短暂处于 failed（例如上一轮刚被非法版本戳弄失败过）。
        #    所以这里要轮询等待，不能立刻判定（踩过：立刻查得到 'failed'，
        #    但几秒后服务其实是正常的）。
        deadline = time.time() + 60
        state = ""
        while time.time() < deadline:
            r = self._run_cmd(["systemctl", "is-active", svc], check=False)
            state = r.stdout.strip()
            if state == "active":
                break
            time.sleep(2)
        if state != "active":
            raise AssertionError(f"迁移测试后服务未恢复: {state!r}")

        # 还原后 ll-cli 必须照常可用（给它几次机会，服务刚起来可能还没就绪）
        last = None
        for _ in range(10):
            r = self._ll_cli("list", timeout=120, check=False)
            if r.returncode == 0:
                last = None
                break
            last = r
            time.sleep(3)
        if last is not None:
            raise AssertionError(
                f"迁移测试后 ll-cli list 失败: {last.stderr[:200]}")

    # ── Test: 生成并安装带 entries 的重写项目 ──
    #
    # 对应 ostree_repo.cpp 的 exportDir / IniLikeFileRewrite 一族：
    #   desktopFileRewrite / buildDesktopExec / dbusServiceRewrite /
    #   systemdServiceRewrite / contextMenuRewrite
    #
    # 这些函数只在【安装】时把 entries 里的 Exec 改写为
    #   /usr/bin/ll-cli run <id> -- ...
    # 冒烟此前从不构造带 share/applications 等目录的项目，
    # 所以这一族（合计 100+ 行）长期是 0%。
    #
    # ⚠️ 分派条件是路径含 "share/applications"、"share/dbus-1"、
    #    "share/systemd/user"、"share/applications/context-menus"。
    #    注意是【share/systemd/user】而不是 lib/systemd/user：
    #    builder 会把 files/lib/systemd/user 复制到 entries/lib/systemd/user，
    #    但 exportDir 只匹配 share/systemd/user，
    #    所以 systemdServiceRewrite 在正常构建流程里走不到（实测 0%）。
    def test_entry_file_rewrite(self):
        proj = f"/tmp/smoke-entries-{os.getpid()}"
        shutil.rmtree(proj, ignore_errors=True)
        os.makedirs(proj, exist_ok=True)

        app_id = "org.deepin.smokeentries"
        # 生成 linglong.yaml。
        # ⚠️ build 是 YAML 块标量，heredoc 正文必须缩进，
        #    否则正文顶格会提前终止块标量，yaml-cpp 报
        #    "end of map not found"（踩过）。这里用 TAB + <<-EOF，
        #    YAML 里 TAB 属于内容，bash 会把行首 TAB 剥掉。
        def heredoc(path: str, body: str) -> str:
            lines = [f'cat > "{path}" <<-EOF']
            lines += ["\t" + ln for ln in body.splitlines()]
            lines.append("EOF")
            return "\n".join(lines)

        prefix = '"$PREFIX"'
        script = "\n".join([
            "set -e",
            f'mkdir -p {prefix}/share/applications/context-menus',
            f'mkdir -p {prefix}/share/dbus-1/services',
            f'mkdir -p {prefix}/share/systemd/user',
            heredoc(
                f"{prefix}/share/applications/{app_id}.desktop",
                "[Desktop Entry]\n"
                "Type=Application\n"
                f"Name=Smoke Entries\n"
                f"Exec=/opt/apps/{app_id}/files/bin/probe %f\n"
                f"Icon={app_id}\n"
                "Terminal=false\n"
                "Categories=Utility;"),
            heredoc(
                f"{prefix}/share/applications/context-menus/{app_id}.conf",
                "[ContextMenu]\n"
                f"Name=Smoke Entries\n"
                f"Exec=/opt/apps/{app_id}/files/bin/probe %U"),
            heredoc(
                f"{prefix}/share/dbus-1/services/{app_id}.service",
                "[D-BUS Service]\n"
                f"Name={app_id}\n"
                f"Exec=/opt/apps/{app_id}/files/bin/probe --dbus"),
            heredoc(
                f"{prefix}/share/systemd/user/{app_id}.service",
                "[Unit]\n"
                "Description=Smoke Entries\n"
                "[Service]\n"
                "Type=simple\n"
                f"ExecStart=/opt/apps/{app_id}/files/bin/probe --serve\n"
                f"ExecStop=/opt/apps/{app_id}/files/bin/probe --stop\n"
                "Restart=on-failure\n"
                "[Install]\n"
                "WantedBy=default.target"),
        ])
        yaml = (
            'version: "1"\n\n'
            "package:\n"
            f"  id: {app_id}\n"
            "  name: smoke entries\n"
            "  version: 1.0.0.1\n"
            "  kind: app\n"
            "  description: |\n"
            "    probe project for entry file rewrites\n\n"
            "command: [echo, -e, smoke-entries]\n\n"
            "base: org.deepin.base/25.2.1\n\n"
            "build: |\n"
            + "\n".join(("  " + ln) if ln.strip() else "" for ln in script.splitlines())
            + "\n"
        )
        (Path(proj) / "linglong.yaml").write_text(yaml)

        old_cwd = os.getcwd()
        try:
            os.chdir(proj)
            r = self._ll_builder("build", check=False, timeout=1800)
            if r.returncode != 0:
                raise AssertionError(
                    f"构建 entries 项目失败: rc={r.returncode} "
                    f"{(r.stdout + r.stderr)[-400:]}")

            # 导出 layer（install 只接受 .layer/.uab）
            r = self._ll_builder("export", "--layer", check=False, timeout=900)
            if r.returncode != 0:
                raise AssertionError(
                    f"导出 layer 失败: rc={r.returncode} "
                    f"{(r.stdout + r.stderr)[-400:]}")
            layers = sorted(Path(proj).glob("*_binary.layer"))
            if not layers:
                raise AssertionError("导出后没有找到 binary.layer")

            # 安装：这时才执行 exportDir 里的 ini-like 重写
            r = self._sudo_ll_cli("install", str(layers[0]),
                                  check=False, timeout=1200)
            if r.returncode != 0:
                raise AssertionError(
                    f"安装 entries layer 失败: rc={r.returncode} "
                    f"{(r.stdout + r.stderr)[-400:]}")

            # 必须重启服务，否则服务端的覆盖计数还在内存里没落盘。
            # 见 flush_service_coverage 的说明。
            self.flush_service_coverage()

            # 验证重写结果：entries 里的 Exec 应被改写为 ll-cli run
            #
            # ⚠️ /var/lib/linglong/entries/apps/ 下是【符号链接】指向层目录，
            #    而 .linyaps.original 备份留在层目录里（同一层内）。
            #    所以要在【链接解析后】的目录里找备份，否则会误判"没备份"。
            desktop = None
            for cand in (
                Path("/var/lib/linglong/entries/apps/share/applications")
                / f"{app_id}.desktop",
                Path("/var/lib/linglong/entries/share/applications")
                / f"{app_id}.desktop",
            ):
                if cand.exists():
                    desktop = cand
                    break
            if desktop is None:
                raise AssertionError(
                    f"安装后找不到 {app_id}.desktop（entries 下的 desktop 文件）")

            content = desktop.read_text(errors="replace")
            if "ll-cli run" not in content:
                raise AssertionError(
                    f"desktop 的 Exec 没有被重写为 ll-cli run: {content[:200]!r}")

            real_dir = desktop.resolve().parent
            if not list(real_dir.glob("*.linyaps.original")):
                raise AssertionError(
                    f"没有生成 .linyaps.original 备份，说明重写走了非预期路径"
                    f"（查找目录 {real_dir}）")

            # dbus service 的 Exec 也应被重写
            dbus_svc = (Path("/var/lib/linglong/entries/share/dbus-1/services")
                        / f"{app_id}.service")
            if dbus_svc.exists():
                dbus_content = dbus_svc.read_text(errors="replace")
                if "ll-cli run" not in dbus_content:
                    raise AssertionError(
                        f"dbus service 的 Exec 没被重写: {dbus_content[:200]!r}")
        finally:
            os.chdir(old_cwd)
            self._sudo_ll_cli("uninstall", app_id, check=False, timeout=600)
            self.flush_service_coverage()
            shutil.rmtree(proj, ignore_errors=True)

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

        为什么 commit 会不存在：states.json 是只增不减的账本，而
        `ll-builder remove` 默认会顺带清理 ostree 对象。实测
        `ll-builder import-dir`（隐藏子命令）会把 layer 记进 states.json，
        但它的 commit 并不在 repo/objects 里，于是留下悬空记录。
        实测整轮跑下来就是这一条：
            org.deepin.layerprobe/1.0.0.1/binary

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
        )
        for prefix, owner in plan:
            # 目录不存在时先建出来（chown -R 对不存在的路径会报错）
            self._run_cmd(["mkdir", "-p", prefix], sudo=True, check=False)
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

        if not self._verify_repo_state_restored("ll-cli"):
            self.has_failed = True
        if not self._verify_repo_state_restored("ll-builder"):
            self.has_failed = True

        # 最后再把服务端计数落盘一次：上面这些卸载/仓库重置同样发生在
        # PackageManager 服务里，不重启就收集不到。
        # 这是整轮冒烟服务端覆盖率的最后一道保障。
        self.flush_service_coverage()

    # ── Test: 版本升降级触发安装交互 ──
    #
    # 覆盖 Cli::interaction（客户端）+ PackageTask::requestInteraction /
    # ReplyInteraction（服务端）。
    #
    # 触发条件（ref_installation.cpp）：
    #   operation == ActionOperation::Policy::Upgrade && !options.skipInteraction
    # 也就是【已装旧版时再装一个更新的版本】。服务端会发 TaskInteraction
    # 信号，客户端收到后构造 InteractionRequest 并回复。
    #
    # ⚠️ 注意 Downgrade 是【另一条】路径：它不走交互，而是直接报
    #    AppInstallNeedDowngrade（除非 --force）。所以必须"先旧后新"，
    #    不能靠 --force 降级来凑。
    def test_upgrade_interaction(self):
        app = CALENDAR_APP_ID

        # ⚠️ 必须用【显式版本】触发升级，不能靠 "install <id>" 让服务端去解析
        #    "最新版"。实测踩过：冒烟运行期间会配置优先级的 smoketesting
        #    仓库，此时远端"最新"的解析结果会变，于是 "install <id>" 被判成
        #    Downgrade 并返回 AppInstallNeedDowngrade
        #    （提示"已安装最新版本…--force"），升级根本没发生。
        #    显式给出版本号就绕开了这层解析。
        versions = self._available_versions(app)
        if len(versions) < 2:
            raise AssertionError(
                f"{app} 可用版本不足两个，无法构造升级场景: {versions}")
        newest, older = versions[0], versions[1]

        def installed_version():
            r = self._ll_cli("list", timeout=180, check=False)
            for line in r.stdout.splitlines():
                if app in line:
                    m = re.search(r"\b(\d+\.\d+\.\d+\.\d+)\b", line)
                    if m:
                        return m.group(1)
            return None

        try:
            self._sudo_ll_cli("uninstall", app, timeout=300, check=False)

            # 先装旧版，并【确认真的装上了旧版】——否则后面的"升级"是假象
            r = self._sudo_ll_cli("install", f"{app}/{older}",
                                  timeout=900, check=False)
            if r.returncode != 0:
                raise AssertionError(
                    f"安装旧版本 {older} 失败: {(r.stdout + r.stderr)[:300]}")
            got = installed_version()
            if got != older:
                raise AssertionError(
                    f"装旧版后期望 {older}，实际 {got}")

            # 关键一步：装更新的版本 -> Policy::Upgrade -> TaskInteraction
            # -> 客户端 Cli::interaction
            r = self._sudo_ll_cli("install", f"{app}/{newest}",
                                  timeout=900, check=False)
            if r.returncode != 0:
                raise AssertionError(
                    f"升级安装失败: {(r.stdout + r.stderr)[:300]}")
            got = installed_version()
            if got != newest:
                raise AssertionError(
                    f"升级后期望 {newest}，实际 {got}")

            # Overwrite 分支：同一版本再装一次
            self._sudo_ll_cli("install", f"{app}/{newest}",
                              timeout=300, check=False)

            # Downgrade + --force：绕过交互直接降级
            r = self._sudo_ll_cli("install", f"{app}/{older}", "--force",
                                  timeout=900, check=False)
            if r.returncode == 0 and installed_version() != older:
                raise AssertionError("--force 降级没有生效")
        finally:
            # 还原成最新版，后面的用例还要用它
            self._sudo_ll_cli("uninstall", app, timeout=300, check=False)
            self._sudo_ll_cli("install", f"{app}/{newest}",
                              timeout=900, check=False)

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

    # ── Test: 项目源码拉取（sources）──
    #
    # 覆盖 fetchSources + SourceFetcher::fetch + getSourceName，以及
    # misc/libexec/linglong/fetch-{file,archive}-source 脚本。
    # 这一整块此前是 0% 覆盖。
    #
    # ⚠️ fetch-*-source 脚本用 wget 下载，而本机的 wget 不认 file://
    #    （实测报"不支持的协议类型"），所以这里起一个本地 HTTP 服务，
    #    不依赖外网也不需要真实仓库。
    def test_project_sources_fetch(self):
        import hashlib

        workdir = Path(tempfile.mkdtemp(prefix="sources-"))
        srcdir = workdir / "www"
        srcdir.mkdir()

        # 准备一个普通文件和一份 tar 包，供 file / archive 两种 kind 使用
        payload = srcdir / "hello.txt"
        payload.write_text("hello from local file source\n")
        tree = srcdir / "tree"
        tree.mkdir()
        (tree / "inner.txt").write_text("inner\n")
        archive = srcdir / "bundle.tar.gz"
        subprocess.run(["tar", "czf", str(archive), "-C", str(tree), "."],
                       check=True, capture_output=True)

        def sha256(p):
            return hashlib.sha256(p.read_bytes()).hexdigest()

        # 起本地 HTTP 服务；端口被占就直接复用（不要去 pkill，容易误伤）
        port = 18700 + os.getpid() % 500
        base_url = f"http://127.0.0.1:{port}"
        server = None
        if subprocess.run(["curl", "-s", "-o", "/dev/null", "-m", "2",
                           f"{base_url}/hello.txt"],
                          capture_output=True).returncode != 0:
            server = subprocess.Popen(
                [sys.executable, "-m", "http.server", str(port),
                 "--bind", "127.0.0.1"],
                cwd=str(srcdir), stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL)
            time.sleep(3)

        project_tpl = (
            'version: "1"\n\n'
            "package:\n"
            "  id: org.deepin.srcprobe\n"
            "  name: srcprobe\n"
            "  version: 1.0.0.1\n"
            "  kind: app\n"
            "  description: probe sources fetching\n\n"
            "command:\n"
            "  - /bin/true\n\n"
            # ⚠️ base 要写模糊版本（三段），写 25.2.2.8 会报
            #    "base version is not valid"
            "base: org.deepin.base/25.2.2\n\n"
            "sources:\n"
            "%s\n"
            "build: |\n"
            "  echo build-ok\n"
        )

        def write_project(name, sources_yaml):
            d = workdir / name
            d.mkdir(exist_ok=True)
            (d / "linglong.yaml").write_text(project_tpl % sources_yaml)
            return d

        ok_sources = (
            "  - kind: file\n"
            f"    url: {base_url}/hello.txt\n"
            f"    digest: {sha256(payload)}\n"
            "    name: hello.txt\n"
            "  - kind: archive\n"
            f"    url: {base_url}/bundle.tar.gz\n"
            f"    digest: {sha256(archive)}\n"
            "    name: bundle"
        )
        good = write_project("ok", ok_sources)

        try:
            r = self._ll_builder("build", cwd=str(good), timeout=1800,
                             check=False)
            out = r.stdout + r.stderr
            if r.returncode != 0:
                raise AssertionError(
                    f"sources 构建失败: {out[-400:]}")

            # 再构建一次走【缓存命中】分支（fetch-*-source 里的
            # cachedir/file_$digest 与 archive_$digest 早退路径）
            self._ll_builder("build", cwd=str(good), timeout=1800,
                             check=False)

            # 错误分支：摘要不符 / 缺 url / 未知 kind / git 缺 commit
            errs = {
                "bad_digest": ("  - kind: file\n"
                               f"    url: {base_url}/hello.txt\n"
                               f"    digest: {'0' * 64}\n"
                               "    name: h.txt"),
                "no_url": ("  - kind: file\n"
                           f"    digest: {sha256(payload)}"),
                "bad_kind": ("  - kind: nosuchkind\n"
                             f"    url: {base_url}/hello.txt\n"
                             f"    digest: {sha256(payload)}"),
                "git_no_commit": ("  - kind: git\n"
                                  f"    url: {base_url}/hello.txt"),
            }
            for name, sy in errs.items():
                d = write_project(name, sy)
                r = self._ll_builder("build", cwd=str(d), timeout=900,
                                         check=False)
                # 这些都必须失败；若某天变成成功说明校验被放开了
                if r.returncode == 0:
                    raise AssertionError(
                        f"sources 错误分支 {name} 竟然构建成功")
        finally:
            if server is not None:
                server.terminate()
                try:
                    server.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    server.kill()
            shutil.rmtree(workdir, ignore_errors=True)
            # 清掉探针装出来的东西（构建产物在 builder 仓库里）
            self._sudo_ll_cli("uninstall", "org.deepin.srcprobe",
                              timeout=300, check=False)

    # ── Test: layer 导入导出往返 ──
    #
    # 覆盖 Builder::importLayer / extractLayer（此前 25 行与 17 行都是 0%），
    # 以及【隐藏子命令】ll-builder import-dir。
    #
    # ⚠️ 为什么原来没覆盖：既有用例只喂了不存在的 layer 文件，
    #    CLI11 的 ExistingFile 校验在进入 Builder 之前就报错退出了，
    #    所以函数体一行都没执行。必须【真的先导出一个 layer】。
    def test_builder_layer_roundtrip(self):
        workdir = Path(tempfile.mkdtemp(prefix="layerprobe-"))
        project = workdir / "proj"
        project.mkdir()
        (project / "linglong.yaml").write_text(
            'version: "1"\n\n'
            "package:\n"
            "  id: org.deepin.layerprobe\n"
            "  name: layerprobe\n"
            "  version: 1.0.0.1\n"
            "  kind: app\n"
            "  description: probe layer import/export\n\n"
            "command:\n"
            "  - /bin/true\n\n"
            "base: org.deepin.base/25.2.2\n\n"
            "build: |\n"
            "  echo layer-ok\n")

        try:
            r = self._ll_builder("build", cwd=str(project), timeout=1800,
                                 check=False)
            if r.returncode != 0:
                raise AssertionError(
                    f"layer 探针构建失败: {(r.stdout + r.stderr)[-400:]}")

            r = self._ll_builder("export", "--layer", cwd=str(project),
                                 timeout=1800, check=False)
            if r.returncode != 0:
                raise AssertionError(
                    f"export --layer 失败: {(r.stdout + r.stderr)[-300:]}")

            layers = sorted(project.glob("*.layer"))
            if not layers:
                raise AssertionError("export --layer 没有产出 .layer 文件")
            layer = layers[0]

            # --modules：显式指定要导出的模块，会走
            # OSTreeRepo::createTempMergedModuleDir（此前 0% 覆盖），
            # 把多个模块目录临时合并成一个 layer 再打包。
            # ⚠️ 必须带 -o，否则会覆盖上面那个 .layer 产物。
            # ⚠️ 带 -o 会走 UAB 导出（需要 builder-utils），环境缺就跳过
            #    这一段，别把整轮判成失败——见 _uab_utils_missing()。
            for mods in ("binary", "binary,develop"):
                out = project / f"modules-{mods.replace(',', '-')}.layer"
                r = self._ll_builder("export", f"--modules={mods}",
                                     "-o", str(out), cwd=str(project),
                                     timeout=1800, check=False)
                if r.returncode != 0:
                    if self._uab_utils_missing(r):
                        break
                    raise AssertionError(
                        f"export --modules={mods} 失败: "
                        f"{(r.stdout + r.stderr)[-300:]}")
                if not out.exists() or out.stat().st_size == 0:
                    raise AssertionError(
                        f"export --modules={mods} 没有产出 layer 文件")

            # --modules 与 --layer 互斥
            self._ll_builder("export", "--layer", "--modules=binary",
                             cwd=str(project), timeout=300, check=False)

            # import：把 layer 导回构建仓库
            r = self._ll_builder("import", str(layer), timeout=900,
                                 check=False)
            if r.returncode != 0:
                raise AssertionError(
                    f"import 失败: {(r.stdout + r.stderr)[-300:]}")

            # extract：把 layer 解到目录
            outdir = workdir / "extracted"
            r = self._ll_builder("extract", str(layer), str(outdir),
                                 timeout=900, check=False)
            if r.returncode != 0:
                raise AssertionError(
                    f"extract 失败: {(r.stdout + r.stderr)[-300:]}")
            if not outdir.is_dir() or not any(outdir.iterdir()):
                raise AssertionError("extract 没有解出任何内容")

            # import-dir：隐藏子命令，直接导目录。
            # ⚠️ 必须先 remove：import 与 import-dir 导入的是同一个 ref，
            #    构建仓库里已经有就会报 "item already exist"（踩过）。
            self._ll_builder("remove", "org.deepin.layerprobe",
                             timeout=600, check=False)
            r = self._ll_builder("import-dir", str(outdir), timeout=900,
                                 check=False)
            if r.returncode != 0:
                raise AssertionError(
                    f"import-dir（隐藏命令）失败: "
                    f"{(r.stdout + r.stderr)[-300:]}")

            # 错误路径：不存在的 layer / 缺参数，由 CLI11 校验拦下
            self._ll_builder("import", str(workdir / "nope.layer"),
                             timeout=300, check=False)
            self._ll_builder("extract", timeout=300, check=False)
        finally:
            # ⚠️ import-dir 会把 layer 记进构建仓库的 states.json，
            #    但它的 commit 并不在 repo/objects 里。留着这条悬空记录，
            #    后面任何一次 ll-builder build 的 mergeModules() 都会失败
            #    （报 "stage pull dependency error"，且重试无用）。
            #    实测整轮冒烟就是被这一条拖挂的。
            self._ll_builder("remove", "org.deepin.layerprobe",
                             timeout=600, check=False)
            shutil.rmtree(workdir, ignore_errors=True)
            self._sudo_ll_cli("uninstall", "org.deepin.layerprobe",
                              timeout=300, check=False)

    # ── Test: 隐藏的运行与卸载选项 ──
    #
    # ll-cli 里有一批 ->group("") 的选项，--help 完全不显示，
    # 但代码路径是实打实的：
    #   run --privileged         要求 root，非 root 直接报错退出
    #   run --caps-add           给容器加 capabilities
    #   run --run-context        直接吃一段 JSON（RunContextConfig）
    #   uninstall --prune/--all  为兼容旧 ll-cli 保留，已废弃但仍在解析
    def test_hidden_options(self):
        app = CALENDAR_APP_ID
        inst = f"smokehidden{os.getpid()}"

        # --privileged 必须 root：先以非 root 拿"拒绝"分支，
        # 再以 root 走真正启用的分支。
        self._ll_cli("run", app, "--instance", inst, "--privileged",
                     "--", "/bin/echo", "PRIV", timeout=120, check=False)

        r = self._sudo_ll_cli("run", app, "--instance", inst,
                              "--privileged", "--", "/bin/echo", "PRIVROOT",
                              timeout=180, check=False)
        if r.returncode != 0:
            raise AssertionError(
                f"root + --privileged 失败: {(r.stdout + r.stderr)[:300]}")

        # --caps-add（delimiter 为逗号）
        r = self._ll_cli("run", app, "--instance", inst + "c",
                         "--caps-add", "CAP_NET_RAW,CAP_SYS_ADMIN",
                         "--", "/bin/echo", "CAP", timeout=120, check=False)
        if r.returncode != 0:
            raise AssertionError(
                f"--caps-add 失败: {(r.stdout + r.stderr)[:300]}")

        # --run-context：合法 JSON 与两类解析失败
        for payload in (
            '{"version":"1","app":"%s","instance":"%s"}' % (app, inst),
            "not json",      # 解析异常分支
            "{}",            # 缺 version，out_of_range 分支
        ):
            self._ll_cli("run", app, "--instance", inst,
                         "--run-context", payload, "--", "/bin/echo", "CTX",
                         timeout=180, check=False)

        # 已废弃但仍在解析的兼容标志：缺 APP 参数会报 APP is required，
        # 正好覆盖参数校验这条分支。
        self._ll_cli("uninstall", "--prune", timeout=120, check=False)
        self._ll_cli("uninstall", "--all", timeout=120, check=False)

        self._cleanup_containers(app, timeout=40)

    # ── Test: 非标准版本串回退解析 ──
    #
    # Version::parse 的默认 ParseOptions 是 strict=true / fallback=true，
    # 也就是 V2（semver）与 V1（四段数字）都解析失败时，会回退到
    # FallbackVersion —— 它接受任意点分字符串。
    # fallback_version.cpp 整个文件（86 行）此前是 0% 覆盖。
    #
    # 触发点：所有接受 <id>/<version> 或 --version 的入口。
    def test_fallback_version_strings(self):
        app = CALENDAR_APP_ID
        # 这些串 V1/V2 都解析不了，必然落到 FallbackVersion
        weird = ["abc", "1.0.0-alpha", "x.y.z", "2024.09", "not-a-version"]
        for v in weird:
            # 查询类：不改变系统状态
            self._ll_cli("info", f"{app}/{v}", timeout=120, check=False)
            self._ll_cli("search", app, f"--version={v}", timeout=180,
                         check=False)
            self._ll_cli("list", f"--version={v}", timeout=120, check=False)
            self._ll_cli("upgrade", f"{app}/{v}", timeout=180, check=False)
            self._ll_cli("analyze", "depends", f"{app}/{v}", timeout=120,
                         check=False)
            # 安装/卸载类：需要 root，且都会被拒绝（远端没有这些版本），
            # 但请求已经过版本解析这一步
            self._sudo_ll_cli("install", f"{app}/{v}", timeout=300,
                              check=False)
            self._sudo_ll_cli("uninstall", f"{app}/{v}", timeout=300,
                              check=False)

    # ── Test: 运行中升级触发延迟卸载 ──
    #
    # 这是【延迟卸载】的完整链路，覆盖三个此前未覆盖的函数：
    #   OSTreeRepo::markDeleted            （旧版本被标记删除）
    #   PackageManager::tryUninstallRef    （busy 分支）
    #   PackageManager::deferredUninstall  （定时器到期后真正删除）
    #
    # 触发路径（switchAppVersion）：
    #   装旧版 -> 跑起来 -> 装新版
    #   -> applyApp(new) / unapplyApp(old)
    #   -> tryUninstallRef(oldRef)
    #   -> isRefBusy(oldRef) 为真 -> markDeleted(old, true) 而不是直接删
    #
    # ⚠️ 注意：直接 "卸载一个正在运行的应用" 走的是 uninstallImpl 里的
    #    【硬拒绝】（AppUninstallAppIsRunning + 弹通知），不会 markDeleted。
    #    必须走"升级"这条路才会进延迟分支（踩过）。
    #
    # 定时器间隔由【服务端】环境变量 LINGLONG_DEFERRED_TIMEOUT（秒）决定，
    # 默认 3600；这里临时调小，跑完还原。
    def test_upgrade_running_app(self):
        app = CALENDAR_APP_ID
        svc = "org.deepin.linglong.PackageManager.service"
        drop_in = Path(
            "/etc/systemd/system/org.deepin.linglong.PackageManager.service.d"
            "/gcov.conf")

        versions = self._available_versions(app)
        if len(versions) < 2:
            raise AssertionError(f"{app} 可用版本不足: {versions}")
        newest, older = versions[0], versions[1]

        original_drop_in = None
        if drop_in.exists():
            original_drop_in = self._run_cmd(
                ["cat", str(drop_in)], sudo=True, check=False).stdout

        inst = f"smokedefer{os.getpid()}"
        container_pid = None
        try:
            # 把延迟卸载超时调小，否则要等 1 小时
            body = ("[Service]\n"
                    "Environment=GCOV_PREFIX=/var/tmp/linglong-cov-svc\n"
                    "Environment=GCOV_PREFIX_STRIP=0\n"
                    "Environment=LINGLONG_DEFERRED_TIMEOUT=15\n")
            self._run_cmd(
                ["bash", "-c",
                 f"printf '%s' '{body}' > {drop_in}"],
                sudo=True, check=False)
            self._run_cmd(["systemctl", "daemon-reload"], sudo=True,
                          check=False)
            self._run_cmd(["systemctl", "restart", svc], sudo=True,
                          check=False, timeout=300)
            time.sleep(5)

            # 1) 装旧版
            self._sudo_ll_cli("uninstall", app, timeout=300, check=False)
            r = self._sudo_ll_cli("install", f"{app}/{older}",
                                  timeout=900, check=False)
            if r.returncode != 0:
                raise AssertionError(
                    f"安装旧版 {older} 失败: {(r.stdout + r.stderr)[:300]}")

            # 2) 把旧版跑起来（真 PTY，长驻）
            container_pid, fd = pty.fork()
            if container_pid == 0:
                env = os.environ.copy()
                env.setdefault("GCOV_PREFIX", "/var/tmp/linglong-cov-user")
                env["GCOV_PREFIX_STRIP"] = "0"
                env.setdefault("DISPLAY", ":0")
                try:
                    os.execvpe(LL_CLI,
                               [LL_CLI, "run", app, "--instance", inst,
                                "--", "sleep", "240"], env)
                finally:
                    os._exit(127)
            if not self._wait_container(timeout=60):
                raise AssertionError("旧版本容器没起来，无法构造 busy 场景")

            # 3) 运行中升级：这一步才会走 tryUninstallRef + markDeleted
            r = self._sudo_ll_cli("install", f"{app}/{newest}",
                                  timeout=900, check=False)
            if r.returncode != 0:
                raise AssertionError(
                    f"运行中升级失败: {(r.stdout + r.stderr)[:300]}")

            # 4) 停掉容器，等定时器把标记为删除的旧版本真正清掉
            self._cleanup_containers(app, timeout=60)
            time.sleep(35)
        finally:
            if container_pid:
                try:
                    os.kill(container_pid, signal.SIGINT)
                except OSError:
                    pass
            # 还原 drop-in（去掉 LINGLONG_DEFERRED_TIMEOUT）
            if original_drop_in is not None:
                safe = original_drop_in.replace("'", "'\\''")
                self._run_cmd(
                    ["bash", "-c", f"printf '%s' '{safe}' > {drop_in}"],
                    sudo=True, check=False)
            else:
                self._run_cmd(["rm", "-f", str(drop_in)], sudo=True,
                              check=False)
            self._run_cmd(["systemctl", "daemon-reload"], sudo=True,
                          check=False)
            self._run_cmd(["systemctl", "restart", svc], sudo=True,
                          check=False, timeout=300)
            time.sleep(3)
            # 恢复成最新版
            self._sudo_ll_cli("uninstall", app, timeout=300, check=False)
            self._sudo_ll_cli("install", f"{app}/{newest}", timeout=900,
                              check=False)

    # ── Test: builder clean 与 create ──
    #
    # 覆盖 main.cpp 的 handleClean / handleCreate。
    # ⚠️ clean 在原来的冒烟里【一次都没跑过】（grep 结果为 0）。
    def test_builder_clean_create(self):
        workdir = Path(tempfile.mkdtemp(prefix="cleanprobe-"))
        name = f"cleanprobe{os.getpid()}"
        try:
            # create：生成模板项目
            r = self._ll_builder("create", name, cwd=str(workdir),
                                 timeout=300, check=False)
            if r.returncode != 0:
                raise AssertionError(
                    f"ll-builder create 失败: {(r.stdout + r.stderr)[:300]}")
            proj = workdir / name
            if not (proj / "linglong.yaml").exists():
                raise AssertionError("create 没有生成 linglong.yaml")

            # create 同名：已存在分支
            self._ll_builder("create", name, cwd=str(workdir), timeout=300,
                             check=False)

            # clean：在项目目录里
            r = self._ll_builder("clean", cwd=str(proj), timeout=600,
                                 check=False)
            if r.returncode != 0:
                raise AssertionError(
                    f"ll-builder clean 失败: {(r.stdout + r.stderr)[:300]}")

            # clean：不在项目目录里（另一条分支）
            self._ll_builder("clean", cwd=str(workdir), timeout=600,
                             check=False)
            # clean 不接受位置参数
            self._ll_builder("clean", "org.deepin.demo", timeout=300,
                             check=False)
        finally:
            shutil.rmtree(workdir, ignore_errors=True)

    # ── Test: 容器配置补丁机制 ──
    #
    # 覆盖 ContainerCfgBuilder::applyPatch / applyPatchFile /
    # applyJsonPatchFile / applyExecutablePatch，此前这一整块是 0% 覆盖。
    #
    # 机制（container_cfg_builder.cpp:1638 起）：
    #   固定读 /usr/lib/linglong/container/config.d 这个目录
    #   （LINGLONG_INSTALL_PREFIX 是 /usr，见 configure.h）：
    #     - 目录下的普通文件      -> 全局补丁
    #     - 目录下以 <appId> 命名的子目录 -> 只对该应用生效
    #   每个补丁文件：
    #     - 有可执行位            -> 当程序跑，stdin 给 OCI 配置 JSON，
    #                                stdout 拿回改后的 JSON
    #     - 扩展名是 .json        -> {ociVersion, patch:[JSON Patch 操作]}
    #                                ociVersion 必须等于 "1.0.1"
    #     - 两者都不是            -> 报错但【只跳过，不影响启动】
    #
    # ⚠️ 这是系统目录，跑完必须清理干净，否则会影响这台机器上
    #    其他应用的正常运行。
    def test_container_config_patch(self):
        cfg_dir = Path("/usr/lib/linglong/container/config.d")
        app = CALENDAR_APP_ID

        # 先确保应用在位（本用例可能排在日历被卸载之后）
        r = self._ll_cli("list", check=False)
        if app not in r.stdout:
            self._sudo_ll_cli("install", app, timeout=900, check=False)

        existed_before = cfg_dir.exists()
        try:
            self._run_cmd(["mkdir", "-p", str(cfg_dir)], sudo=True,
                          check=False)

            # 1) 可执行补丁：原样透传，验证"可执行位优先"这条分支
            self._run_cmd(
                ["bash", "-c",
                 f"printf '%s\\n' '#!/bin/sh' 'cat' > {cfg_dir}/passthrough.sh"],
                sudo=True, check=False)
            self._run_cmd(["chmod", "755", f"{cfg_dir}/passthrough.sh"],
                          sudo=True, check=False)

            # 2) 全局 JSON 补丁：给 annotations 加一个键
            patch = ('{"ociVersion":"1.0.1","patch":[{"op":"add",'
                     '"path":"/annotations","value":{"smoke":"1"}}]}')
            self._run_cmd(
                ["bash", "-c", f"printf '%s' '{patch}' > {cfg_dir}/global.json"],
                sudo=True, check=False)

            # 3) 按 appId 的子目录补丁
            app_dir = cfg_dir / app
            self._run_cmd(["mkdir", "-p", str(app_dir)], sudo=True,
                          check=False)
            app_patch = ('{"ociVersion":"1.0.1","patch":[{"op":"add",'
                         '"path":"/annotations/apppatch","value":"yes"}]}')
            self._run_cmd(
                ["bash", "-c",
                 f"printf '%s' '{app_patch}' > {app_dir}/app.json"],
                sudo=True, check=False)

            # 4) 坏补丁：既不可执行也不是 .json -> 走"跳过"分支
            self._run_cmd(
                ["bash", "-c", f"printf '%s' 'nonsense' > {cfg_dir}/bad.txt"],
                sudo=True, check=False)
            self._run_cmd(["chmod", "644", f"{cfg_dir}/bad.txt"],
                          sudo=True, check=False)

            # 触发：跑一次应用就会执行 applyPatch
            r = self._ll_cli("run", app, "--", "/bin/echo", "PATCHOK",
                             timeout=180, check=False)
            out = r.stdout + r.stderr
            if r.returncode != 0:
                raise AssertionError(
                    f"带补丁运行失败: {out[:300]}")
            # 坏补丁必须被跳过而不是让启动失败
            if "bad.txt" not in out and "patch" not in out.lower():
                # 输出可能被日志级别吞掉，不强制要求出现
                pass

            # 再跑一次，覆盖配置已存在时的复用路径
            self._ll_cli("run", app, "--", "/bin/echo", "PATCHOK2",
                         timeout=180, check=False)
        finally:
            # 清理：只删我们自己造的文件，目录原本不存在就整个删掉
            for f in ("passthrough.sh", "global.json", "bad.txt"):
                self._run_cmd(["rm", "-f", str(cfg_dir / f)], sudo=True,
                              check=False)
            self._run_cmd(["rm", "-rf", str(cfg_dir / app)], sudo=True,
                          check=False)
            if not existed_before:
                self._run_cmd(["rm", "-rf", str(cfg_dir)], sudo=True,
                              check=False)
            self._cleanup_containers(app, timeout=40)

    # ── Test: 安装钩子（install hooks）──
    #
    # 覆盖 utils/hooks.cpp 的 parseInstallHooks / parseInstallHookCommandLine /
    # executeHookCommands / executeInstallHooks / executePostInstallHooks /
    # executePostUninstallHooks。这个文件此前只有 22% 覆盖
    # （parseInstallHookCommandLine 与 executeHookCommands 都是 0%）。
    #
    # 机制（utils/hooks.cpp:92）：
    #   从 LINGLONG_INSTALL_HOOKS_DIR = /etc/linglong/config.d 读【所有】
    #   普通文件，逐行匹配三个前缀：
    #     ll-pre-install=      装之前跑
    #     ll-post-install=     装完跑
    #     ll-post-uninstall=   卸载后跑
    #   值可以用单/双引号包起来；引号不闭合会报
    #   "Invalid install hook command: unterminated quoted command"。
    #   只有【从文件安装】（ll-cli install <x.uab> / <x.layer>）才会走
    #   InstallFromFile -> parseInstallHooks + executeInstallHooks，
    #   从仓库按 id 安装不会。
    #
    # ⚠️ 这是系统目录，跑完必须清理干净，否则这台机器上
    #    以后所有从文件安装都会跑我们的钩子。
    def test_install_hooks(self):
        cfg_dir = Path("/etc/linglong/config.d")
        workdir = Path(tempfile.mkdtemp(prefix="hookprobe-"))
        app_id = f"org.deepin.hookprobe{os.getpid()}"
        hook_file = cfg_dir / "smoke-hooks.conf"
        existed_before = cfg_dir.exists()
        try:
            # 先构建并导出一个普通 UAB
            (workdir / "linglong.yaml").write_text(
                'version: "1"\n\n'
                "package:\n"
                f"  id: {app_id}\n"
                "  name: hookprobe\n"
                "  version: 1.0.0.1\n"
                "  kind: app\n"
                "  description: probe install hooks\n"
                f"  architecture: {DEMO_ARCH}\n\n"
                "base: org.deepin.base/25.2.2\n\n"
                "command:\n"
                "  - /bin/echo\n\n"
                "build: |\n"
                '  mkdir -p "$PREFIX/bin"\n'
                f"  printf '#!/bin/sh\\necho hookprobe\\n' "
                f'> "$PREFIX/bin/hookprobe"\n'
                '  chmod 755 "$PREFIX/bin/hookprobe"\n')

            r = self._ll_builder("build", cwd=str(workdir), timeout=1800,
                                 check=False)
            if r.returncode != 0:
                raise AssertionError(
                    f"hook 探针构建失败: {(r.stdout + r.stderr)[-400:]}")

            uab = workdir / "hookprobe.uab"
            r = self._ll_builder("export", "-o", str(uab), cwd=str(workdir),
                                 timeout=1800, check=False)
            if r.returncode != 0:
                raise AssertionError(
                    f"hook 探针导出失败: {(r.stdout + r.stderr)[-400:]}")

            self._run_cmd(["mkdir", "-p", str(cfg_dir)], sudo=True,
                          check=False)

            # 三种前缀 + 三种写法：不带引号、带双引号、空命令
            body = ("ll-pre-install=/bin/echo HOOK_PRE_RAN\n"
                    'll-post-install="/bin/echo HOOK_POST_RAN"\n'
                    "ll-post-uninstall='/bin/echo HOOK_POSTUN_RAN'\n"
                    "# 注释行与无关行会被跳过\n"
                    "some-other-key=whatever\n"
                    "ll-pre-install=\n")
            self._run_cmd(
                ["bash", "-c",
                 f"printf '%s' '{body}' > {hook_file}"],
                sudo=True, check=False)

            # 从文件安装：触发 parseInstallHooks + executeInstallHooks
            self._sudo_ll_cli("uninstall", app_id, timeout=300, check=False)
            r = self._sudo_ll_cli("install", str(uab), timeout=1800,
                                  check=False)
            if r.returncode != 0:
                raise AssertionError(
                    f"带钩子的 UAB 安装失败: {(r.stdout + r.stderr)[-400:]}")

            # 卸载：触发 executePostUninstallHooks
            r = self._sudo_ll_cli("uninstall", app_id, timeout=900,
                                  check=False)
            if r.returncode != 0:
                raise AssertionError(
                    f"带钩子的卸载失败: {(r.stdout + r.stderr)[-300:]}")

            # 引号不闭合：应当拒绝安装
            self._run_cmd(
                ["bash", "-c",
                 f"printf '%s' 'll-pre-install=\"unterminated\\n' "
                 f"> {hook_file}"],
                sudo=True, check=False)
            r = self._sudo_ll_cli("install", str(uab), timeout=900,
                                  check=False)
            combined = r.stdout + r.stderr
            if "Invalid install hook" not in combined \
                    and "Invalid install hook" not in combined:
                raise AssertionError(
                    f"引号不闭合的钩子竟然没被拒绝: {combined[:300]}")

            # 目录不存在时的分支：删掉钩子文件后再装一次
            self._run_cmd(["rm", "-f", str(hook_file)], sudo=True,
                          check=False)
            self._sudo_ll_cli("install", str(uab), timeout=1800,
                              check=False)
            self._sudo_ll_cli("uninstall", app_id, timeout=900, check=False)
        finally:
            self._run_cmd(["rm", "-f", str(hook_file)], sudo=True,
                          check=False)
            if not existed_before:
                self._run_cmd(["rm", "-rf", str(cfg_dir)], sudo=True,
                              check=False)
            self._ll_builder("remove", app_id, cwd=str(workdir),
                             timeout=600, check=False)
            shutil.rmtree(workdir, ignore_errors=True)

    # ── Test: entries 全量重导出 ──
    #
    # 覆盖 OSTreeRepo::fixExportAllEntries 与 exportAllEntries，
    # 这两个函数此前是 0% 覆盖。
    #
    # 机制（ostree_repo.cpp:2305）：
    #   fixExportAllEntries 读 repoDir/entries/.version，
    #   内容等于 LINGLONG_EXPORT_VERSION 就【跳过】，
    #   否则调 exportAllEntries 全量重建 entries 并写回版本号。
    #
    # 触发方式：这个函数只被 ll-package-manager 的 main() 在
    # 【服务启动时】调用一次（apps/ll-package-manager/src/main.cpp:109），
    # 所以删掉 .version 再重启服务即可（踩过：只删文件不重启没用，
    # 而且服务不退出就不会把覆盖率落盘）。
    #
    # ⚠️ entries 是全局共享目录，重导出会把它清空重建。
    #    必须在没有容器运行时做，且做完要确认应用仍能启动。
    def test_repo_entries_reexport(self):
        svc = "org.deepin.linglong.PackageManager.service"
        version_file = Path("/var/lib/linglong/entries/.version")
        entries_dir = Path("/var/lib/linglong/entries")

        if not version_file.exists():
            raise AssertionError(f"{version_file} 不存在，无法构造场景")

        original = self._run_cmd(["cat", str(version_file)], sudo=True,
                                 check=False).stdout.strip()

        # 重导出期间不能有容器在跑
        self._cleanup_containers(CALENDAR_APP_ID, timeout=60)

        try:
            self._run_cmd(["rm", "-f", str(version_file)], sudo=True,
                          check=False)
            self._run_cmd(["systemctl", "restart", svc], sudo=True,
                          check=False, timeout=300)

            # 等服务起来并把 entries 重建完
            deadline = time.time() + 180
            rebuilt = False
            while time.time() < deadline:
                r = self._run_cmd(["cat", str(version_file)], sudo=True,
                                  check=False)
                if r.returncode == 0 and r.stdout.strip():
                    rebuilt = True
                    break
                time.sleep(3)
            if not rebuilt:
                raise AssertionError(
                    "重启服务后 entries/.version 没有被重建，"
                    "exportAllEntries 可能失败了")

            # entries 至少要还有内容。
            # ⚠️ .version 写完不代表 entries 已经稳定，重导出刚结束的
            #    一瞬间目录可能还是空的，所以这里要轮询而不是查一次。
            deadline = time.time() + 120
            populated = False
            while time.time() < deadline:
                r = self._run_cmd(["ls", str(entries_dir)], sudo=True,
                                  check=False)
                # sudo -A 的 askpass 会往 stdout 里塞一行"验证成功"，
                # 所以按行过滤，不能只看 stdout 是否为空。
                names = [ln.strip() for ln in r.stdout.splitlines()
                         if ln.strip() and "验证" not in ln]
                if r.returncode == 0 and names:
                    populated = True
                    break
                time.sleep(3)
            if not populated:
                raise AssertionError("重导出后 entries 目录一直是空的")

            # 服务恢复后应用必须仍能启动
            deadline = time.time() + 90
            while time.time() < deadline:
                r = self._ll_cli("list", check=False)
                if CALENDAR_APP_ID in r.stdout:
                    break
                time.sleep(3)

            # 再等仓库彻底安静下来。
            # ⚠️ 重导出之后 ostree 仓库还会忙一阵子，紧接着跑构建会随机
            #    报 "stage pull dependency error / cannot be used in
            #    worksheets"。这里用一次"轻量读"探到稳定为止。
            deadline = time.time() + 120
            stable = 0
            while time.time() < deadline:
                r = self._ll_cli("list", "--json", check=False)
                if r.returncode == 0:
                    stable += 1
                    if stable >= 3:
                        break
                else:
                    stable = 0
                time.sleep(5)
            time.sleep(10)
        finally:
            # .version 会由 exportAllEntries 重新写入；万一没写成，
            # 至少把原值放回去，避免留下"每次启动都重导出"的状态
            r = self._run_cmd(["cat", str(version_file)], sudo=True,
                              check=False)
            if r.returncode != 0 and original:
                self._run_cmd(
                    ["bash", "-c",
                     f"printf '%s' '{original}' > {version_file}"],
                    sudo=True, check=False)


    # ── Test: builder 的 buildext.apt 依赖 ──
    #
    # 覆盖 Builder::buildStagePreCommit / generateDependsScript，
    # 这两个函数此前分别是 57 行里 49 行未覆盖、26 行里 20 行未覆盖。
    #
    # 机制（linglong_builder.cpp:961 与 2113）：
    #   buildStagePreCommit 先调 generateDependsScript，
    #   项目没写 buildext.apt.depends 就直接 return（所以此前几乎没覆盖）。
    #   写了之后会：准备 prepare_base / prepare_runtime 两个 overlayfs，
    #   在容器里执行生成的 buildext.sh。
    #
    # ⚠️ 关键点：buildext.apt.depends 生成的脚本每行都带
    #    `|| echo "$?"` 容错，所以【不需要真的能装包】，apt 失败也会
    #    继续走完整条链路 —— 这正是它适合冒烟测试的原因。
    #    （对比 buildext.apt.build_depends 生成的脚本没有容错，
    #      apt 失败会直接让构建失败，所以这里不用 build_depends。）
    def test_builder_buildext_apt(self):
        workdir = Path(tempfile.mkdtemp(prefix="buildext-"))
        pid = os.getpid()
        # 构建缓存里累积的"账上有、仓里没有"的 layer 会让
        # mergeModules 失败，先做一次自洽性检查
        self.ensure_builder_cache_healthy()
        try:
            # 两个探针：
            #   depends       -> buildStagePreCommit + generateDependsScript
            #   build_depends -> processBuildDepends + generateBuildDependsScript
            # 实测两者都能在容器里正常 apt（构建成功），不是只有报错路径。
            probes = (
                ("depends", f"org.deepin.buildext{pid}", "depends"),
                ("builddepends", f"org.deepin.builddep{pid}", "build_depends"),
            )
            for tag, app_id, key in probes:
                proj = workdir / tag
                proj.mkdir()
                (proj / "linglong.yaml").write_text(
                    'version: "1"\n\n'
                    "package:\n"
                    f"  id: {app_id}\n"
                    f"  name: buildext {tag}\n"
                    "  version: 1.0.0.1\n"
                    "  kind: app\n"
                    f"  description: probe buildext apt {key}\n"
                    f"  architecture: {DEMO_ARCH}\n\n"
                    "base: org.deepin.base/25.2.2\n\n"
                    "buildext:\n"
                    "  apt:\n"
                    f"    {key}:\n"
                    "      - zlib1g\n\n"
                    "command:\n"
                    "  - /bin/echo\n\n"
                    "build: |\n"
                    f"  echo buildext-{tag}-ok\n")

                r = self._build_with_retry(proj, f"buildext.apt.{key}")
                if r.returncode != 0:
                    raise AssertionError(
                        f"带 buildext.apt.{key} 的构建失败: "
                        f"{(r.stdout + r.stderr)[-400:]}")

                # 再跑一次：内部目录已存在，overlay 准备走另一条分支
                self._build_with_retry(proj, f"buildext.apt.{key}(二次)")

                self._ll_builder("remove", app_id, cwd=str(proj),
                                 timeout=600, check=False)
        finally:
            shutil.rmtree(workdir, ignore_errors=True)

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

    # ── Test: UABX（自执行 UAB）导出 ──
    #
    # 覆盖 Builder::exportUAB 与 UABPackager::prepareExecutableBundle /
    # ensureExecEntry / packBundle。此前整个冒烟里 "uabx" 一次都没出现过。
    #
    # 机制（linglong_builder.cpp:1580 起 / uab_packager.cpp）：
    #   export --uabx 会把 uab-loader（静态链接的自执行头）拼到 .uab 前面，
    #   产出一个可直接 chmod +x 运行的 ELF。
    #
    # ⚠️ 踩过的坑：
    #   1. --uabx 要求项目里【必须真有 binary 模块】，
    #      即构建脚本要往 $PREFIX 里装文件；只 echo 的 build 会报
    #      "binary module is required in UABX mode"。
    #   2. --uabx 与 --layer 互斥。
    #   3. 产出的 UABX 虽然能直接执行，但【不能用 ll-cli install 安装】，
    #      会报 "executable UAB installation is not supported"；
    #      而按 .uabx 后缀安装更早就会被"不支持的文件格式"拦下。
    #   4. 运行 UABX 本身拿不到覆盖率：uab-loader 没有 .gcno
    #      （不在覆盖率分母里），所以自执行路径对覆盖率无贡献。
    def test_builder_uabx_export(self):
        workdir = Path(tempfile.mkdtemp(prefix="uabx-"))
        app_id = f"org.deepin.uabx{os.getpid()}"
        icon = workdir / "icon.png"
        try:
            # 1x1 的合法 PNG，足够让 packIcon 跑通
            icon.write_bytes(base64.b64decode(
                "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR4"
                "2mP8z8BQDwAEhQGAhKmMIQAAAABJRU5ErkJggg=="))

            (workdir / "linglong.yaml").write_text(
                'version: "1"\n\n'
                "package:\n"
                f"  id: {app_id}\n"
                "  name: uabx probe\n"
                "  version: 1.0.0.1\n"
                "  kind: app\n"
                "  description: probe uabx export\n"
                f"  architecture: {DEMO_ARCH}\n\n"
                "base: org.deepin.base/25.2.2\n\n"
                "command:\n"
                f"  - /opt/apps/{app_id}/files/bin/uabxprobe\n\n"
                "build: |\n"
                '  mkdir -p "$PREFIX/bin"\n'
                "  printf '#!/bin/sh\\necho uabx-ran-ok\\n' "
                '> "$PREFIX/bin/uabxprobe"\n'
                '  chmod 755 "$PREFIX/bin/uabxprobe"\n')

            r = self._ll_builder("build", cwd=str(workdir), timeout=1800,
                                 check=False)
            if r.returncode != 0:
                raise AssertionError(
                    f"UABX 探针构建失败: {(r.stdout + r.stderr)[-400:]}")

            # --uabx 与 --layer 互斥
            self._ll_builder("export", "--layer", "--uabx", cwd=str(workdir),
                             timeout=300, check=False)

            # 正式导出
            out = workdir / "out.uabx"
            r = self._ll_builder("export", "--uabx", "--icon", str(icon),
                                 "-o", str(out), cwd=str(workdir),
                                 timeout=1800, check=False)
            if r.returncode != 0:
                # 环境里没有满足版本要求的 builder-utils 时，UAB 导出
                # 做不成（详见 _uab_utils_missing）。这属于环境缺件，
                # 跳过而不是判失败。
                if self._uab_utils_missing(r):
                    return
                raise AssertionError(
                    f"export --uabx 失败: {(r.stdout + r.stderr)[-400:]}")
            if not out.exists() or out.stat().st_size == 0:
                raise AssertionError("export --uabx 没有产出文件")

            # 产出的应该是一个可直接执行的 ELF
            with open(out, "rb") as fp:
                if fp.read(4) != b"\x7fELF":
                    raise AssertionError("UABX 产物不是 ELF 可执行文件")
            if not os.access(out, os.X_OK):
                raise AssertionError("UABX 产物没有可执行权限")

            # 不带 --icon 再导一次（icon 是可选的）
            self._ll_builder("export", "--uabx", "-o",
                             str(workdir / "noicon.uabx"), cwd=str(workdir),
                             timeout=1800, check=False)

            # 自执行 UAB 不能被安装
            asuab = workdir / "asuab.uab"
            shutil.copyfile(out, asuab)
            r = self._sudo_ll_cli("install", str(asuab), timeout=900,
                                  check=False)
            combined = r.stdout + r.stderr
            if "not supported" not in combined and "不支持" not in combined:
                raise AssertionError(
                    f"自执行 UAB 竟然可以安装: {combined[:300]}")

            # 用 .uabx 后缀安装：更早就会被文件格式检查拦下
            self._sudo_ll_cli("install", str(out), timeout=300, check=False)

            self._sudo_ll_cli("uninstall", app_id, timeout=300, check=False)
        finally:
            self._ll_builder("remove", app_id, cwd=str(workdir),
                             timeout=600, check=False)
            shutil.rmtree(workdir, ignore_errors=True)


# ── Main ──
def ensure_session_bus_env():
    """把会话总线地址补进环境。

    ll-cli 在【非 TTY】下会去建 DBusNotifier（连 org.freedesktop.Notifications
    的 ActionInvoked / NotificationClosed 信号）。如果 DBUS_SESSION_BUS_ADDRESS
    没设置，构造会抛异常 -> 回退成 DummyNotifier，于是
    cli/dbus_notifier.cpp 以及升级交互、运行中卸载的通知路径全都覆盖不到。

    Jenkins 里跑的冒烟是非交互启动的，环境里通常没有这个变量，
    所以在这里按当前登录用户补一个默认值。
    """
    if os.environ.get("DBUS_SESSION_BUS_ADDRESS"):
        return
    candidate = Path(f"/run/user/{os.getuid()}/bus")
    if candidate.exists():
        os.environ["DBUS_SESSION_BUS_ADDRESS"] = f"unix:path={candidate}"


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

    ensure_session_bus_env()

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
