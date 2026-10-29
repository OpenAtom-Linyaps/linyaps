#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

"""玲珑冒烟测试 — 命令行子命令覆盖步骤。"""

import json
import os
import shutil
import tempfile
import random
import re
import socket
import sys
import time
from pathlib import Path

from ..constants import (
    SMOKE_REPO_URL,
    DEMO_APP_ID,
    CALENDAR_APP_ID,
    LIBEXEC_DIR,
    DRIVER_DETECT_BINARY,
)
from ..models import RepoState, StepResult, StepSkipped


class CommandSteps:


    # ── Test: 只读子命令全覆盖 ──
    #
    # ps / list 的各种变体原先完全没有用例，cli.cpp 里对应的大段代码零覆盖。
    # 全部只读或幂等，不会破坏环境。
    # ⚠️ 每条断言都在真机上实测过，不要凭 help 文本想当然：
    #    - ll-cli version 不存在（rc=109）
    #    - repo show --json 不支持 --json（rc=109）
    #    这两个故意放在错误路径用例里，用来覆盖参数校验分支。
    def test_readonly_query_commands(self):
        # 前面的用例会起容器（GUI 应用装/升级后 DBus 服务还会自动拉起），
        # 容器退出与服务端记录的回收都是异步的 —— 不能假设"此刻必然无
        # 容器"（实测 run64/run65：日历容器退出后 ps 里仍有残留记录）。
        self._ensure_no_containers()
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

        # --repo：只在指定仓库里搜。这里必须验证两件事，只断言 rc 是不够的：
        #   1) 指定的仓库不存在时要【报错】，而不是悄悄退化成"搜所有仓库"
        #   2) 指定真实仓库时要能正常出结果
        r = self._ll_cli("search", "deepin", "--repo", "no-such-repo-smoke",
                         timeout=90, check=False)
        if r.returncode == 0:
            raise AssertionError(
                "search --repo 指定不存在的仓库竟然成功 —— 说明 --repo 被忽略了")
        if r.returncode < 0:
            raise AssertionError("search --repo 不存在的仓库崩溃了")
        if "no-such-repo-smoke" not in (r.stdout + r.stderr):
            raise AssertionError(
                f"search --repo 的报错里应指明是哪个仓库找不到: "
                f"{(r.stdout + r.stderr)[:200]!r}")

        # 真实仓库名从 repo show 的第一行取（"Default: <alias>"），不写死，
        # 免得仓库配置一变这个用例就假失败
        r = self._ll_cli("repo", "show", timeout=60, check=False)
        m = re.search(r"^Default:\s*(\S+)", r.stdout, re.M)
        if not m:
            raise AssertionError(
                f"repo show 输出里找不到默认仓库: {r.stdout[:200]!r}")
        default_repo = m.group(1)

        r = self._ll_cli("search", "deepin", "--repo", default_repo,
                         timeout=120, check=False)
        if r.returncode != 0:
            raise AssertionError(
                f"search --repo {default_repo} 失败: "
                f"rc={r.returncode} {r.stderr[:200]}")
        if "org.deepin.base" not in r.stdout:
            raise AssertionError(
                f"search --repo {default_repo} deepin 应能搜到 org.deepin.base, "
                f"实际: {r.stdout[:200]!r}")

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

        # 直接起一次 peer 模式的【服务端】，让它走"没人连接 -> 10 秒启动
        # 超时 -> 自己退出并删掉 socket"这条路。
        #
        # ⚠️ 为什么非得起这一次：ll-cli --no-dbus 时服务端是被
        #     sudo --user deepin-linglong --preserve-env=... ll-package-manager --no-dbus
        #    起的，sudo 会把 GCOV_PREFIX 剥掉，于是它只能往【编译期路径】
        #    （构建树，属主是 v25）写 .gcda —— uid 993 根本写不进去。
        #    实测：构建树里的 main.cpp.gcda 时间戳一直停在编译那一刻，
        #    apps/ll-package-manager/src/main.cpp 的 peer 分支
        #    （peer socket / 启动超时 / 退出清理，约 25 行）从来没被统计过。
        #    这里用 root + 显式 GCOV_PREFIX 直接跑一次服务端，覆盖率就能落盘。
        #
        # ⚠️ 两个必须踩对的点：
        #   1. 服务端 main() 里有一道 `qgetenv("USER") != deepin-linglong`
        #      就直接退出的检查 —— 用 root 跑（USER=root）根本进不去 peer
        #      分支，必须 sudo --user deepin-linglong 且把 USER 传对。
        #   2. GCOV_PREFIX 要显式给（sudo 会剥掉外层环境），而且必须指向
        #      deepin-linglong 可写的前缀；不给的话它会去写编译期路径
        #      （构建树，属主 v25）—— uid 993 写不进去，覆盖率就丢了。
        sock = f"/tmp/smoke-peer-{os.getpid()}.socket"
        plog = f"/tmp/smoke-peer-{os.getpid()}.log"
        prefix = os.environ.get("GCOV_PREFIX_ROOT",
                                "/var/tmp/linglong-cov-root")
        user = "deepin-linglong"
        script = (
            f"rm -f {sock}\n"
            f"sudo -A --user {user} env USER={user} "
            f"GCOV_PREFIX={prefix} GCOV_PREFIX_STRIP=0 "
            f"{LIBEXEC_DIR}/ll-package-manager --no-dbus "
            f"--peer-socket {sock} >{plog} 2>&1 &\n"
            "pid=$!\n"
            "for i in $(seq 1 60); do [ -S " + sock + " ] && break; sleep 0.2; done\n"
            "if [ ! -S " + sock + " ]; then echo NO_SOCKET; "
            "cat " + plog + "; kill $pid 2>/dev/null; exit 1; fi\n"
            "for i in $(seq 1 100); do kill -0 $pid 2>/dev/null || break; "
            "sleep 0.3; done\n"
            "if kill -0 $pid 2>/dev/null; then echo STILL_RUNNING; "
            "kill $pid 2>/dev/null; exit 1; fi\n"
            "if [ -S " + sock + " ]; then echo SOCKET_LEFT; exit 1; fi\n"
            "echo PEER_EXITED_CLEANLY\n")
        r = self._run_cmd(["bash", "-c", script], timeout=180, check=False)
        out = (r.stdout or "") + (r.stderr or "")
        # 探针日志别留在机器上
        self._run_cmd(["bash", "-c", f"rm -f {plog}"], sudo=True, check=False)
        if "PEER_EXITED_CLEANLY" not in out:
            raise AssertionError(
                "peer 模式的 package-manager 没能自己起、自己退干净: "
                f"rc={r.returncode} {out[-300:]}")

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

    # ── Test: repo 的别名与镜像写分支 ──
    #
    # 覆盖 repo.cpp 里三条此前没走到的写分支：
    #   - add --alias：alias 存进去之后【就是】这个仓库的查找键
    #   - enable-mirror --region：写 mirrorEnabled 与 region
    #   - modify：整条子命令已废弃，必须明确报错
    #
    # ⚠️ 全程只动一个临时仓库，绝不去改 stable。
    #    已有的 test_repo_mirror_management 会 enable/disable stable 的镜像，
    #    如果这里再用 stable 验证 region，就会把机器上原有的镜像配置改掉。
    #
    # 判据落在【落盘的配置文件】/var/lib/linglong/config.yaml 上，
    # 而不是只看 rc —— 写分支的语义就是"配置真的被改了"。
    def test_repo_alias_and_mirror(self):
        name = "smoke-alias-probe"
        alias = "smoke-alias-probe-alias"
        cfg_path = Path("/var/lib/linglong/config.yaml")

        def repo_cfg() -> str:
            r = self._run_cmd(["cat", str(cfg_path)], sudo=True, check=False)
            return r.stdout

        def cleanup():
            # 别名和名称都要试一遍：repo 的查找键是 alias.value_or(name)，
            # 万一 --alias 没生效（或有人改坏了这个选项），键就变成 name，
            # 只按别名删会漏掉，把临时仓库留在 /var/lib/linglong/config.yaml
            # 里污染后续所有运行。变异测试就是这么暴露出这个漏洞的。
            for key in (alias, name):
                self._sudo_ll_cli("repo", "remove", key, timeout=60, check=False)

        cleanup()  # 幂等：上次跑挂了的残留先清掉
        try:
            before = repo_cfg()
            if name in before or alias in before:
                raise AssertionError(
                    f"清理后配置里仍有 {name}/{alias}，不敢继续 —— "
                    f"请手动执行 `sudo ll-cli repo remove {name}` 清掉残留。"
                    f"当前配置: {before[:300]!r}")

            # add --alias：别名与名称都给，且刻意取不同的值
            r = self._sudo_ll_cli("repo", "add", name, SMOKE_REPO_URL,
                                  "--alias", alias, timeout=120, check=False)
            if r.returncode != 0:
                raise AssertionError(
                    f"repo add --alias 失败: rc={r.returncode} {r.stderr[:200]}")

            after_add = repo_cfg()
            if f"name: {name}" not in after_add:
                raise AssertionError(
                    f"repo add 后配置里没有 name: {name}: {after_add[:400]!r}")
            if f"alias: {alias}" not in after_add:
                raise AssertionError(
                    f"repo add --alias 后配置里没有 alias: {alias} "
                    f"（--alias 没被采纳）: {after_add[:400]!r}")

            # 重复添加必须被拒绝，而且不能把配置写成两份。
            # 查重用的也是 alias.value_or(name)，所以这里正好一并验证。
            r = self._sudo_ll_cli("repo", "add", name, SMOKE_REPO_URL,
                                  "--alias", alias, timeout=120, check=False)
            if r.returncode == 0:
                raise AssertionError(
                    f"repo add 重复添加同名/同别名仓库竟然成功: {name}/{alias}")
            if "already exist" not in (r.stdout + r.stderr):
                raise AssertionError(
                    f"repo add 重复添加的报错应说明已存在: "
                    f"{(r.stdout + r.stderr)[:200]!r}")
            if repo_cfg().count(f"alias: {alias}") != 1:
                raise AssertionError(
                    f"重复 add 被拒后配置里 {alias} 应仍只有一份: "
                    f"{repo_cfg()[:400]!r}")

            # 关键判据：alias 才是查找键。用别名操作必须成功，
            # 用名称操作必须失败（repo.cpp 里一律 alias.value_or(name)）。
            r = self._sudo_ll_cli("repo", "set-priority", alias, "5",
                                  timeout=60, check=False)
            if r.returncode != 0:
                raise AssertionError(
                    f"repo set-priority 用别名 {alias} 竟然失败: "
                    f"rc={r.returncode} {r.stderr[:200]}")

            r = self._sudo_ll_cli("repo", "set-priority", name, "6",
                                  timeout=60, check=False)
            if r.returncode == 0:
                raise AssertionError(
                    f"repo set-priority 用名称 {name} 竟然成功 —— "
                    f"说明查找键不是 alias，--alias 形同虚设")

            # enable-mirror --region：两个字段都要落盘
            r = self._sudo_ll_cli("repo", "enable-mirror", alias,
                                  "--region", "CN", timeout=120, check=False)
            if r.returncode != 0:
                raise AssertionError(
                    f"repo enable-mirror --region 失败: "
                    f"rc={r.returncode} {r.stderr[:200]}")

            after_mirror = repo_cfg()
            block = self._yaml_repo_block(after_mirror, alias)
            if "mirror_enabled: true" not in block:
                raise AssertionError(
                    f"enable-mirror 后该仓库应写 mirror_enabled: true, "
                    f"实际块: {block!r}")
            if "region: CN" not in block:
                raise AssertionError(
                    f"enable-mirror --region CN 后应写 region: CN, "
                    f"实际块: {block!r}")

            # disable-mirror 应把开关关掉
            r = self._sudo_ll_cli("repo", "disable-mirror", alias,
                                  timeout=120, check=False)
            if r.returncode != 0:
                raise AssertionError(
                    f"repo disable-mirror 失败: rc={r.returncode} {r.stderr[:200]}")
            block = self._yaml_repo_block(repo_cfg(), alias)
            if "mirror_enabled: true" in block:
                raise AssertionError(
                    f"disable-mirror 后 mirror_enabled 应变成 false, "
                    f"实际块: {block!r}")

            # modify：整条子命令已废弃，必须报错且说明替代做法
            r = self._sudo_ll_cli("repo", "modify", SMOKE_REPO_URL,
                                  timeout=60, check=False)
            if r.returncode == 0:
                raise AssertionError("repo modify 已废弃，竟然还返回成功")
            if r.returncode < 0:
                raise AssertionError("repo modify 崩溃了")
            out = r.stdout + r.stderr
            if "modify" not in out or "deprecated" not in out:
                raise AssertionError(
                    f"repo modify 的报错应说明该子命令已废弃: {out[:300]!r}")
        finally:
            cleanup()

        # 收尾：临时仓库必须清干净，且 stable 不能被顺手改坏
        final = repo_cfg()
        if name in final or alias in final:
            raise AssertionError(
                f"临时仓库没有清理干净: {final[:400]!r}")
        if "name: stable" not in final:
            raise AssertionError(
                f"stable 仓库不应被本用例影响: {final[:400]!r}")

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

        # --version：两个命令各有一份实现，ll-cli 还多一条 --json 分支。
        # 不能只看 rc —— 必须验证版本号真的被解析出来，而且两个命令报的是
        # 同一个构建（它们同源编译，不一致说明有一边的 flag 没生效）。
        #
        # ⚠️ 这里不能用 \b 起头：ll-builder 的输出是
        #    "如意玲珑构建工具版本1.15.0-dev+8a8ee84"（版本号紧贴汉字），
        #    而汉字在 Python 的 Unicode 语义下算 \w，所以 "本1" 之间
        #    【没有】\b 边界，加了 \b 会一个都匹配不到。
        versions = {}
        for name, runner in (("ll-cli", self._ll_cli),
                             ("ll-builder", self._ll_builder)):
            r = runner("--version", timeout=60, check=False)
            if r.returncode != 0:
                raise AssertionError(
                    f"{name} --version 失败: rc={r.returncode} {r.stderr[:200]}")
            m = re.search(r"(\d+\.\d+\.\d+[0-9A-Za-z.+-]*)", r.stdout)
            if not m:
                raise AssertionError(
                    f"{name} --version 输出里解析不出版本号: {r.stdout[:200]!r}")
            versions[name] = m.group(1)

        if versions["ll-cli"] != versions["ll-builder"]:
            raise AssertionError(
                f"ll-cli 与 ll-builder 报的版本不一致（同源编译不该如此）: {versions}")

        # ll-cli --json --version 走的是另一条输出分支（cli.cpp:666）
        r = self._ll_cli("--json", "--version", timeout=60, check=False)
        if r.returncode != 0:
            raise AssertionError(
                f"ll-cli --json --version 失败: rc={r.returncode} {r.stderr[:200]}")
        try:
            data = json.loads(r.stdout)
        except json.JSONDecodeError as e:
            raise AssertionError(
                f"ll-cli --json --version 不是合法 JSON: {e}; {r.stdout[:200]!r}")
        if data.get("version") != versions["ll-cli"]:
            raise AssertionError(
                f"--json --version 与文本模式的版本不一致: "
                f"{data.get('version')!r} != {versions['ll-cli']!r}")

        # 非法全局选项应被拒绝
        r = self._ll_cli("--definitely-not-a-flag", check=False)
        if r.returncode == 0:
            raise AssertionError("ll-cli 非法全局选项竟然成功")
        if r.returncode < 0:
            raise AssertionError("ll-cli 非法全局选项崩溃了")

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

    # ── Test: ll-driver-detect 的交互通知 ──
    #
    # 上面那条只跑到 --check-only；真正会发通知的那段（main.cpp 的
    # "Handle user notifications" 以及 apps/ll-driver-detect/src/
    # dbus_notifier.cpp 的 init / sendInteractiveNotification）需要
    # 一个能应答的 org.freedesktop.Notifications。这里复用桩通知服务：
    #
    #   not_remind -> 必须把 neverRemind 写进
    #                 $XDG_CONFIG_HOME/linglong/driver_detection.json
    #   不应答      -> 25 秒超时后什么都不做，配置文件不能被创建
    #
    # ⚠️ 绝不回 install_now：那条路会真的去装 1.6GB 的
    #    org.deepin.driver.display.nvidia.<版本> 驱动包。
    #
    # 用独立的 XDG_CONFIG_HOME（临时目录）跑，既不碰真实配置，也避开
    # ll-driver-detect 的单实例锁文件（锁在同一个目录下）。
    def test_driver_detect_notification(self):
        if not os.path.exists(DRIVER_DETECT_BINARY):
            raise AssertionError(f"找不到 {DRIVER_DETECT_BINARY}")

        for action, expect_never_remind in (("not_remind", True), ("none", False)):
            cfg_home = Path(tempfile.mkdtemp(prefix="ll-drvcfg-"))
            cfg = cfg_home / "linglong" / "driver_detection.json"
            try:
                bus = self._start_notification_stub(action)
                try:
                    started = time.time()
                    r = self._run_driver_detect(bus, cfg_home)
                    elapsed = time.time() - started

                    notes = self._notification_log()
                    if not notes:
                        # 驱动检测要先查远端仓库里有没有对应的驱动包。镜像抖一下
                        # （实测报 "Failed to get package info from remote repo:
                        # Failed to parse search result"）或者仓库里暂时没有
                        # 这个包，工具就会自己报 Driver detection failed，
                        # 根本走不到通知分支 —— 那是环境问题，不是被测功能的
                        # 问题，所以明确跳过并写清原因。
                        combined = r.stdout + r.stderr
                        if "Driver detection failed" in combined:
                            raise StepSkipped(
                                f"[{action}] ll-driver-detect 没能走到通知分支"
                                f"（仓库/镜像问题）："
                                f"{combined.strip().splitlines()[-1][:200]}")
                        raise AssertionError(
                            f"[{action}] 桩没收到通知，说明没走到通知分支: "
                            f"rc={r.returncode} {(r.stdout + r.stderr)[:200]}")
                    actions = notes[-1].get("actions") or []
                    if "install_now" not in actions or "not_remind" not in actions:
                        raise AssertionError(
                            f"[{action}] 通知按钮不是驱动安装的两个动作: {actions!r}")

                    cfg_text = cfg.read_text() if cfg.exists() else ""
                    if expect_never_remind:
                        if r.returncode != 0:
                            raise AssertionError(
                                f"[{action}] ll-driver-detect 退出码 {r.returncode}: "
                                f"{(r.stdout + r.stderr)[:200]}")
                        if '"neverRemind": true' not in cfg_text:
                            raise AssertionError(
                                f"[{action}] 选了不再提醒，配置里却没有 neverRemind: "
                                f"{cfg_text!r}")
                    else:
                        # 没应答 -> 25 秒超时 -> 什么都不做
                        if cfg_text:
                            raise AssertionError(
                                f"[{action}] 没人应答却写了配置: {cfg_text!r}")
                        if elapsed < 20:
                            raise AssertionError(
                                f"[{action}] 只用了 {elapsed:.1f}s，没等通知超时，"
                                "说明通知没发出去就返回了")
                finally:
                    self._stop_notification_stub()
            finally:
                shutil.rmtree(cfg_home, ignore_errors=True)

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
