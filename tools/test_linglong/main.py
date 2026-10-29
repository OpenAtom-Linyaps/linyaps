#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

"""玲珑冒烟测试 — 入口与步骤表。"""

import os
import sys
import argparse
from pathlib import Path

from .base import SmokeTestBase
from .steps.repo import RepoSteps
from .steps.build import BuildSteps
from .steps.app import AppSteps
from .steps.runtime import RuntimeSteps
from .steps.commands import CommandSteps
from .steps.output import OutputSteps
from .steps.service import ServiceSteps


class SmokeTest(
    SmokeTestBase,
    RepoSteps,
    BuildSteps,
    AppSteps,
    RuntimeSteps,
    CommandSteps,
    OutputSteps,
    ServiceSteps,
):
    STEPS = [
        ("清理并重置仓库", "reset_repositories", "仓库管理"),
        ("记录 ll-cli 和 ll-builder 当前仓库状态", "record_current_repo_state", "仓库管理"),
        (
            "配置冒烟测试仓库",
            "configure_smoke_repositories",
            "仓库管理",
            ("record_current_repo_state",),
        ),
        ("创建 demo 项目", "create_demo_project", "应用构建"),
        (
            "构建并导出 demo 应用",
            "test_demo_build_and_export",
            "应用构建",
            ("create_demo_project",),
        ),
        (
            "验证 demo DBus 环境变量",
            "test_demo_dbus_environment",
            "应用构建",
            ("create_demo_project",),
        ),
        # ⚠️ 这两个用例必须在 demo 项目目录还存在时执行：
        #    步骤「安装并运行 demo 应用」结束时会把 org.deepin.demo/ 删掉，
        #    所以它们必须排在它前面，不能放到末尾。
        (
            "builder build 跳过选项",
            "test_builder_build_options",
            "应用构建",
            ("create_demo_project",),
        ),
        (
            "builder --isolate-network 网络隔离",
            "test_builder_isolate_network",
            "应用构建",
            ("create_demo_project",),
        ),
        (
            "builder export 选项",
            "test_builder_export_options",
            "应用构建",
            ("create_demo_project",),
        ),
        (
            "builder run 选项",
            "test_builder_run_options",
            "应用构建",
            ("create_demo_project",),
        ),
        # push 的错误路径需要在真实项目目录里跑（要读 linglong.yaml），
        # 所以必须和上面几个 builder 用例一样排在「安装并运行 demo 应用」之前
        # —— 那一步结束会把 org.deepin.demo/ 删掉。
        (
            "builder push 错误路径",
            "test_builder_push_errors",
            "构建器覆盖",
            ("create_demo_project",),
        ),
        (
            "安装并运行 demo 应用",
            "test_demo_install_and_run",
            "应用部署",
            ("create_demo_project", "test_demo_build_and_export"),
        ),
        (
            "查询仓库与运行时信息",
            "test_repository_queries",
            "应用部署",
            ("configure_smoke_repositories",),
        ),
        (
            "ll-cli run 运行期选项",
            "test_run_command_options",
            "运行时覆盖",
            ("test_demo_install_and_run",),
        ),
        (
            "容器生命周期 ps/enter/kill",
            "test_container_lifecycle",
            "运行时覆盖",
            ("test_demo_install_and_run",),
        ),
        (
            "容器实例复用",
            "test_container_instance_reuse",
            "运行时覆盖",
            ("test_demo_install_and_run",),
        ),
        # ── 覆盖率补强用例（第一批）──
        # 只依赖 base 与已安装应用，不依赖 demo 构建产物。
        # ⚠️ 必须排在「清理未使用运行时（prune）」之前：prune 会把无人依赖的
        #    base 一并移除，之后 run 就没有可运行目标了。
        (
            "run 的文件/URL/设备传递选项",
            "test_run_file_url_device",
            "运行时覆盖",
            ("test_demo_install_and_run",),
        ),
        (
            "run 的 base/runtime 显式指定",
            "test_run_explicit_base_runtime",
            "运行时覆盖",
            ("test_demo_install_and_run",),
        ),
        ("run 的 debug 选项错误路径", "test_run_debug_options", "运行时覆盖"),
        ("全局选项与帮助输出", "test_global_options", "命令覆盖"),
        ("清理未使用运行时（prune）", "test_prune", "命令覆盖"),
        (
            "search 筛选选项",
            "test_search_options",
            "命令覆盖",
            ("configure_smoke_repositories",),
        ),
        (
            "inspect 目录查询",
            "test_inspect_commands",
            "命令覆盖",
            ("configure_smoke_repositories",),
        ),
        (
            "扩展运行（--extensions）",
            "test_run_with_extensions",
            "运行时覆盖",
            ("test_demo_install_and_run",),
        ),
        (
            "扩展层的解析、env 注入与挂载",
            "test_extension_layer_env_and_mount",
            "运行时覆盖",
            ("configure_smoke_repositories",),
        ),
        (
            "运行配置 env 注入",
            "test_runtime_config_env_injection",
            "运行时覆盖",
            ("test_demo_install_and_run",),
        ),
        ("架构字符串解析", "test_architecture_parsing", "应用管理"),
        ("旧版仓库配置迁移", "test_repo_config_v1_migration", "仓库管理"),
        ("仓库缓存重建", "test_repo_cache_rebuild", "仓库管理"),
        (
            "重复安装与强制重装",
            "test_install_force_and_duplicate",
            "应用管理",
            ("configure_smoke_repositories",),
        ),
        ("缓存写失败时的行为", "test_cache_write_failure", "仓库管理"),
        ("锁目录不可写时的行为", "test_filelock_dir_unwritable", "运行时覆盖"),
        ("容器运行时不可用时的行为", "test_runtime_binary_unusable", "运行时覆盖"),
        (
            "--json 输出一致性",
            "test_json_output_variants",
            "CLI 输出",
            ("configure_smoke_repositories",),
        ),
        (
            "运行配置嵌套挂载",
            "test_runtime_config_mounts",
            "运行时覆盖",
            ("configure_smoke_repositories",),
        ),
        (
            "TTY 透传与信号转发",
            "test_tty_and_signal_forwarding",
            "运行时覆盖",
            ("test_demo_install_and_run",),
        ),
        (
            "从终端复用运行中的实例",
            "test_reuse_instance_with_tty",
            "运行时覆盖",
            ("test_demo_install_and_run",),
        ),
        (
            "客户端在任务进行中断开",
            "test_client_disconnect_during_task",
            "服务管理",
            ("configure_smoke_repositories",),
        ),
        ("错误路径必须给出清晰报错", "test_error_paths_report_clearly", "CLI 输出"),
        (
            "应用 permissions 的 binds/innerBinds",
            "test_run_app_permissions_binds",
            "运行时覆盖",
            ("test_demo_install_and_run",),
        ),
        (
            "安装/升级错误路径",
            "test_install_error_paths",
            "命令覆盖",
            ("configure_smoke_repositories",),
        ),
        (
            "安装、升级并运行日历应用",
            "test_calendar_install_upgrade_run",
            "应用管理",
            ("configure_smoke_repositories",),
        ),
        (
            "验证日历模块生命周期",
            "test_calendar_module_lifecycle",
            "应用管理",
            ("configure_smoke_repositories",),
        ),
        (
            "验证 versionV1 到 versionV2 升降级",
            "test_semver_upgrade_flow",
            "应用管理",
            ("configure_smoke_repositories",),
        ),
        (
            "安装并运行 baseline 测试套件",
            "test_testsuite_baseline",
            "应用管理",
            ("configure_smoke_repositories",),
        ),
        ("版本号解析边界", "test_version_parsing_edge_cases", "应用管理"),
        ("查询只读子命令（ps/list/repo）", "test_readonly_query_commands", "命令覆盖"),
        (
            "检查已安装应用（info/content）",
            "test_installed_app_inspection",
            "命令覆盖",
            ("test_demo_install_and_run",),
        ),
        ("验证错误路径处理", "test_error_paths", "命令覆盖"),
        (
            "分析已安装应用（analyze）",
            "test_analyze_commands",
            "命令覆盖",
            ("test_calendar_install_upgrade_run",),
        ),
        (
            "builder 项目级子命令",
            "test_builder_project_commands",
            "构建器覆盖",
            ("test_demo_build_and_export",),
        ),
        # ── 覆盖率补强用例（第二批）──
        # 安装/卸载/升级的选项分支错误路径，只改参数不改系统状态，放最后安全。
        (
            "install 的仓库与模块选项",
            "test_install_repo_module_options",
            "命令覆盖",
            ("configure_smoke_repositories",),
        ),
        (
            "uninstall 的模块与强制选项",
            "test_uninstall_module_force_options",
            "命令覆盖",
            ("configure_smoke_repositories",),
        ),
        ("upgrade 的依赖与清理选项", "test_upgrade_deps_prune_options", "命令覆盖"),
        # ── 覆盖率补强用例（第三批）──
        # 这三个不依赖 demo 产物，也不改变已安装应用，放最后最安全。
        # peer 模式走 ll-cli 自建的 package-manager，与 dbus 服务是两套代码；
        # --json 走 json_printer；镜像管理走 repo 的写分支。
        (
            "peer 模式（--no-dbus）",
            "test_no_dbus_peer_mode",
            "命令覆盖",
            ("configure_smoke_repositories",),
        ),
        ("仓库镜像管理", "test_repo_mirror_management", "命令覆盖"),
        # repo add --alias / enable-mirror --region / modify 三条写分支。
        # 只动一个临时仓库（用完即删），不碰 stable 的镜像配置。
        ("repo 别名与镜像写分支", "test_repo_alias_and_mirror", "命令覆盖"),
        ("--json 输出格式", "test_json_output", "命令覆盖"),
        # ── 覆盖率补强用例（第四批）──
        # 都在真 PTY 下跑：ll-cli 用 isatty() 决定是否走 TTY 分支，
        # subprocess 的管道会让这些代码永远不被执行。
        # 必须排在日历应用已安装之后（依赖 CALENDAR_APP_ID 可运行）。
        (
            "PTY 下的 run 与容器复用",
            "test_run_in_pty",
            "运行时覆盖",
            ("test_demo_install_and_run",),
        ),
        (
            "run 的文件/URL 占位符映射",
            "test_run_placeholder_mapping",
            "运行时覆盖",
            ("test_demo_install_and_run",),
        ),
        (
            "CDI 设备注入",
            "test_cdi_device_injection",
            "运行时覆盖",
            ("test_calendar_install_upgrade_run",),
        ),
        (
            "显示/时区/网络环境分支",
            "test_display_env_branches",
            "运行时覆盖",
            ("test_calendar_install_upgrade_run",),
        ),
        (
            "--debug 的 gdb 附加提示",
            "test_debug_attach_hint",
            "运行时覆盖",
            ("configure_smoke_repositories",),
        ),
        # ── 覆盖率补强用例（第五批）──
        # driver-detect 是独立程序，不依赖应用；放在 PTY 用例之后。
        ("图形驱动检测工具", "test_driver_detect", "命令覆盖"),
        # 上面那条只跑 --check-only；这条用桩通知服务把"发通知问用户装不装"
        # 这段跑起来：回 not_remind 必须写 neverRemind，不应答必须 25 秒
        # 超时且什么都不做。不碰 install_now（会下载 1.6GB 驱动）。
        ("驱动检测的交互通知", "test_driver_detect_notification", "命令覆盖"),
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
        (
            "版本升降级触发安装交互",
            "test_upgrade_interaction",
            "应用管理",
            ("configure_smoke_repositories",),
        ),
        # install -y -> skipInteraction。对照式验证：PTY 下不带 -y 回答 "n"
        # 必须出现提问且升级被取消；带 -y 必须完全不提问且真的升上去。
        # 依赖日历应用可用，排在上面那条之后。
        (
            "install -y 跳过交互",
            "test_install_yes_option",
            "应用管理",
            ("configure_smoke_repositories",),
        ),
        # 非 TTY 的通知交互：同样的升级场景，但把一条 root 私有会话总线 +
        # 桩通知服务（notif_stub.py）交给 ll-cli，让它走
        # cli/dbus_notifier.cpp 的 GetCapabilities/Notify/等信号，而不是
        # sudo 下退化的 DummyNotifier。桩分别回 yes / no，断言
        # "真的升上去" / "真的被取消"，并核对通知正文里的新旧两个 ref。
        # 自己还原成最新版（后面的 PTY / 容器用例还要用日历应用）。
        (
            "通知交互：yes 升级 / no 取消",
            "test_install_interaction_notification",
            "应用管理",
            ("configure_smoke_repositories",),
        ),
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
        (
            "隐藏的运行与卸载选项",
            "test_hidden_options",
            "运行时覆盖",
            ("test_calendar_install_upgrade_run",),
        ),
        # 非标准版本串：VersionV1/V2 都解析不了时会回退到 FallbackVersion，
        # 整个 fallback_version.cpp 此前是 0% 覆盖。
        ("非标准版本串回退解析", "test_fallback_version_strings", "命令覆盖"),
        # 运行中升级 -> switchAppVersion 移除旧版本 -> tryUninstallRef
        # 发现旧版本 busy -> markDeleted 延迟删除 -> 定时器 deferredUninstall。
        # 覆盖 markDeleted / tryUninstallRef / deferredUninstall。
        (
            "运行中升级触发延迟卸载",
            "test_upgrade_running_app",
            "应用管理",
            ("test_calendar_install_upgrade_run",),
        ),
        # ll-builder 的 clean / create：clean 此前一次都没跑过。
        ("builder clean 与 create", "test_builder_clean_create", "构建器覆盖"),
        # remove --no-clean-objects：判据是对象仓文件数的变化，
        # 自带一个一次性应用，跑完自己清掉引用。
        ("builder remove --no-clean-objects", "test_builder_remove_no_clean_objects", "构建器覆盖"),
        # 容器配置补丁机制：/usr/lib/linglong/container/config.d 下的
        # 可执行补丁与 JSON 补丁（applyPatch / applyJsonPatchFile /
        # applyExecutablePatch），此前整块未覆盖。
        (
            "容器配置补丁机制",
            "test_container_config_patch",
            "运行时覆盖",
            ("configure_smoke_repositories",),
        ),
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
        (
            "entries 全量重导出",
            "test_repo_entries_reexport",
            "仓库覆盖",
            ("test_calendar_install_upgrade_run",),
        ),
    ]

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
    parser.add_argument(
        "--fail-fast",
        action="store_true",
        help="第一个失败后跳过其余步骤（旧行为，用于定位第一个失败点）",
    )
    args = parser.parse_args()

    ensure_session_bus_env()

    print("==========================================")
    print("  玲珑冒烟测试")
    print("==========================================")

    test = SmokeTest(
        dated=args.dated, verbose=args.verbose, fail_fast=args.fail_fast
    )
    test.run()

    if test.has_failed:
        print("\nSmoke testing failed")
        sys.exit(1)
    else:
        print("\n成功执行玲珑冒烟测试")
        sys.exit(0)


if __name__ == "__main__":
    main()
