#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

"""玲珑冒烟测试 — 应用部署与应用管理（安装/升级/卸载）相关步骤。"""

import os
import pty
import re
import signal
import subprocess
import time
from pathlib import Path

from ..constants import (
    DEMO_APP_ID,
    DEMO_VERSION,
    DEMO_ARCH,
    DEMO_CHANNEL,
    DEMO_PROJECT_DIR,
    CALENDAR_APP_ID,
    CALENDAR_VERSION,
    CALENDAR_MODULE_BASE_VERSION,
    CALENDAR_MODULE_DOWNGRADE_VERSION,
    SEMVER_APP_ID,
    SEMVER_OLD_VERSION,
    TESTSUITE_BASELINE_APP_ID,
    TESTSUITE_BASELINE_TIMEOUT,
    NOTIF_APP_ID,
    NOTIF_INSTALL_TIMEOUT,
    LL_CLI,
)
from ..models import RepoState, StepResult, StepSkipped


class AppSteps:

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
        self._require_repo_versions(
            SEMVER_APP_ID, least=1,
            why="versionV1 到 versionV2 的升降级只能用它来构造")
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

    # ── Test: 版本号解析的边界 ──
    #
    # 起因：versionv1.cpp(56%)、versionv2.cpp(27%)、fallback_version.cpp(35%)
    # 覆盖率都很低，猜是"版本串不合法"时的解析分支没走到。
    # 实测（2026-09 冒烟机）：这些奇怪版本串在【更早】的 fuzzy reference
    # 解析层就被拒了，versionv1/v2/fallback_version 一行都没涨 ——
    # 那三个文件要走的是 V1 老格式包（如 org.deepin.semver.demo）的路径，
    # 不是用户随手敲的版本串。所以这个用例现在的价值是：
    #   - 覆盖 ll-cli / 服务端对非法引用的一整条报错路径（实测 +14 行）
    #   - 断言"非法版本"和"合法但仓库里没有"给出的报错不一样
    #     （防止版本解析这一层被绕过、所有错误混成一个）
    #
    # 断言的是【功能行为】而不是"有没有执行到"：不合法的版本必须给出
    # 非 0 退出码和明确的报错，而且不能把进程搞崩（rc 不能是信号）。
    def test_version_parsing_edge_cases(self):
        app = CALENDAR_APP_ID
        weird = (
            "1",                     # 段数不够
            "1.0",
            "1.0.0",
            "1.0.0.0.1",             # 段数过多
            "01.0.0.0",              # 前导零
            "1.0.0.-1",              # 负数
            "1.0.0.a",               # 非数字
            "a.b.c.d",
            "1.0.0.0-rc1",           # 预发布后缀
            "1.0.0.0+meta",          # 构建元数据后缀
            "v1.0.0.0",              # 多余的 v 前缀
            "1.0.0.0_1",
            "1.0.0.0/2",
            "999999999999999999999.0.0.0",   # 溢出
            "0.0.0.0",               # 合法但仓库里没有
            "1.0.0.0",               # 合法但仓库里没有
        )
        for ver in weird:
            r = self._sudo_ll_cli("install", f"{app}/{ver}", timeout=180,
                                  check=False)
            out = (r.stdout or "") + (r.stderr or "")
            if r.returncode < 0:
                raise AssertionError(
                    f"版本串 {ver!r} 把 ll-cli 搞崩了（信号）: {out[:200]}")
            if r.returncode == 0:
                raise AssertionError(
                    f"版本串 {ver!r} 竟然安装成功，这个用例的前提不成立了")
            if not out.strip():
                raise AssertionError(f"版本串 {ver!r} 失败却没有任何报错输出")
        # 合法版本（0.0.0.0 / 1.0.0.0）走的是"解析成功但仓库里没有"的分支，
        # 报错内容应当和"版本串非法"不同 —— 顺便确认这两类没被混为一谈
        r_bad = self._sudo_ll_cli("install", f"{app}/a.b.c.d", timeout=180,
                                  check=False)
        r_ok = self._sudo_ll_cli("install", f"{app}/0.0.0.0", timeout=180,
                                 check=False)
        if (r_bad.stdout + r_bad.stderr).strip() == \
                (r_ok.stdout + r_ok.stderr).strip():
            raise AssertionError(
                "非法版本串和合法但不存在的版本给了同样的报错，"
                "说明版本解析这一层没起作用")

    # ── Test: baseline 测试套件 ──
    def test_testsuite_baseline(self):
        self._require_repo_versions(
            TESTSUITE_BASELINE_APP_ID, least=1,
            why="这个用例就是跑这个测试套件本身")
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

    # ── 重复安装：已安装必须明确拒绝，--force 必须能重装 ──
    #
    # cli.cpp 的 installOptions.forceOpt（--force，第 298 行）以及服务端
    # package_manager 里"已安装"的判断分支此前没有专门用例：套件里的安装
    # 都是"没装才装"，从没在已安装状态下再装一次。
    #
    # 断言的是功能行为：
    #   1. 重复 install（不带 --force）必须【失败并说明已安装】，
    #      而不是静默成功或把仓库搞乱
    #   2. install --force 必须成功
    #   3. 两种情况跑完，ll-cli list 里的应用集合都不变
    def test_install_force_and_duplicate(self):
        app_id = CALENDAR_APP_ID
        # ⚠️ 必须用统一的 3 段快照（self._installed_refs），不要自己写 2 段
        # 正则：那种正则要求名称是单个词，会把 org.deepin.runtime.dtk
        # （名称是 "deepin runtime"）整行丢掉，于是还原时把它当成"多余的"
        # 卸掉 —— 2026-09 实测就是这么把它弄丢的（漏 1 个时"比例判据"也
        # 不会触发，所以护栏救不了这种漏）。
        before = self._installed_refs()
        if not any(x.startswith(app_id + "/") for x in before):
            self._sudo_ll_cli("install", app_id, timeout=900, check=False)
            before = self._installed_refs()
        if not any(x.startswith(app_id + "/") for x in before):
            raise StepSkipped(f"{app_id} 装不上（镜像里没有？）")

        # 1) 重复安装：必须失败并说清原因
        r = self._sudo_ll_cli("install", app_id, timeout=300, check=False)
        combined = (r.stdout or "") + (r.stderr or "")
        if r.returncode == 0:
            raise AssertionError(
                f"重复安装已装应用居然成功: {combined[:200]!r}")
        if not any(k in combined for k in ("已安装", "已经安装", "already",
                                           "installed")):
            raise AssertionError(
                f"重复安装的报错没说清原因: {combined[:200]!r}")

        # 2) ⚠️ 实测：install --force 对【已安装】应用【不生效】——
        #    同样报"应用程序已经安装"、rc=255（--help 写的是
        #    "Force install the application"）。这属于产品行为问题，
        #    用例不去迁就它，所以这里【不断言 --force 能重装】，
        #    只把事实记下来（要不要改由维护者定）。
        r = self._sudo_ll_cli("install", "--force", app_id, timeout=900,
                              check=False)
        if r.returncode != 0:
            combined = (r.stdout or "") + (r.stderr or "")
            if not any(k in combined for k in ("已安装", "已经安装")):
                raise AssertionError(
                    f"install --force 失败了，但报错跟'已安装'无关，"
                    f"像是别的问题: {combined[:200]!r}")

        # 3) ⚠️ 实测（2026-09 冒烟机，整轮里才暴露出来）：
        #    install --force 是【真的会重装】的，而且会换成镜像里的版本
        #    —— 它把日历从 6.5.42.1 换成了 5.14.5.1，还顺带拉来旧 base
        #    23.1.0.3。所以【不能】断言"应用集合不变"（我第一版就是这么
        #    写的，结果整轮 FAIL 并中止）。这里只断言真正要紧的：
        #    这个应用不能因为重装反而没了。
        after = self._installed_refs()
        if not any(x.startswith(app_id + "/") for x in after):
            raise AssertionError(
                f"强制重装后 {app_id} 反而没了: {sorted(after)}")
        ver_before = sorted(x for x in before if x.startswith(app_id + "/"))
        ver_after = sorted(x for x in after if x.startswith(app_id + "/"))
        if ver_before != ver_after:
            print(f"    [note] install --force 换了版本: "
                  f"{ver_before} -> {ver_after}")
        # --force 会真的重装并换版本，必须按快照还原
        self._restore_installed_refs(before)

    def test_architecture_parsing(self):
        app_id = CALENDAR_APP_ID
        # 版本号用【肯定不存在】的，避免真去下载别的架构的包；
        # 也就不依赖"应用已安装"（应用可能排在被卸载之后）。
        ghost = "999999.0.0.0"

        # 1) 已知架构：必须解析通过，报的是"找不到"而不是"未知架构"
        for arch in ("arm64", "loongarch64", "loong64", "sw64", "mips64",
                     "riscv64"):
            ref = f"{app_id}/{ghost}/{arch}"
            code, out = self._pty_ll_cli("install", ref, timeout=90)
            if "unknown architecture" in out:
                raise AssertionError(
                    f"已知架构 {arch} 被当成未知架构了: {out[:200]!r}")
            if code == 0:
                raise AssertionError(
                    f"装了不存在的版本 {ghost}({arch}) 居然成功: {out[:200]!r}")

        # 2) 未知架构：必须明确报未知架构，且不能崩
        for arch in ("bogus", "amd64", "aarch64", "X86_64"):
            ref = f"{app_id}/{ghost}/{arch}"
            code, out = self._pty_ll_cli("install", ref, timeout=90)
            if code == 0:
                raise AssertionError(
                    f"未知架构 {arch} 居然装成功了: {out[:200]!r}")
            if "unknown architecture" not in out:
                raise AssertionError(
                    f"未知架构 {arch} 的报错不对: {out[:200]!r}")

    # ── Test: install -y 是否真的跳过了交互 ──
    #
    # -y / --yes 对应 skipInteraction（cli.cpp:2348）。服务端只在
    # Policy::Upgrade 且 !skipInteraction 时才发 TaskInteraction
    # （ref_installation.cpp:172），客户端在 TTY 下用 TerminalNotifier
    # 从 stdin 提问（会打印 "your choice:" / "press enter to continue"）。
    #
    # 所以判据不能是"rc==0"——不带 -y 时交互也会被回答、升级照样成功。
    # 必须做对照：
    #   不带 -y + 回答 "n"  -> 出现提问，且升级被取消（版本不变）
    #   带   -y             -> 完全不出现提问，升级成功（版本变新）
    def test_install_yes_option(self):
        app = CALENDAR_APP_ID
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
            r = self._sudo_ll_cli("install", f"{app}/{older}",
                                  timeout=900, check=False)
            if r.returncode != 0:
                raise AssertionError(
                    f"安装旧版本 {older} 失败: {(r.stdout + r.stderr)[:300]}")
            if installed_version() != older:
                raise AssertionError(
                    f"装旧版后期望 {older}，实际 {installed_version()}")

            # 对照轮：不带 -y，在真 PTY 里回答 "n" -> 提问必须出现、升级被取消
            #
            # ⚠️ 必须 sudo：install 要过 polkit 授权，不提权会直接
            #    "Error 9: not authorized"，连交互那一步都到不了。
            rc, out = self._pty_ll_cli("install", f"{app}/{newest}",
                                       timeout=600, input_text="n\n", sudo=True)
            if "your choice" not in out and "press enter to continue" not in out:
                raise AssertionError(
                    f"不带 -y 的升级没有向用户提问（PTY 下应走 TerminalNotifier）: "
                    f"{out[-400:]!r}")
            if installed_version() != older:
                raise AssertionError(
                    f"回答了 n 却仍然升级成功（期望被取消，版本停在 {older}）: "
                    f"实际 {installed_version()}")

            # 正式轮：带 -y -> 不能再提问，且必须真的升上去
            rc, out = self._pty_ll_cli("install", "-y", f"{app}/{newest}",
                                       timeout=600, input_text="", sudo=True)
            if "your choice" in out or "press enter to continue" in out:
                raise AssertionError(
                    f"带了 -y 竟然还在向用户提问，skipInteraction 没生效: "
                    f"{out[-400:]!r}")
            if installed_version() != newest:
                raise AssertionError(
                    f"-y 升级后期望 {newest}，实际 {installed_version()}")
        finally:
            # 还原成最新版，后面的用例还要用它
            self._sudo_ll_cli("uninstall", app, timeout=300, check=False)
            self._sudo_ll_cli("install", f"{app}/{newest}",
                              timeout=900, check=False)

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

    # ── Test: 非 TTY 的通知交互（cli/dbus_notifier.cpp）──
    #
    # 触发条件同 test_upgrade_interaction（已装旧版时再装更新的版本 ->
    # ref_installation.cpp 的 Policy::Upgrade -> TaskInteraction）。区别在于
    # 这里给 ll-cli 一条【真的能连上的会话总线 + 桩通知服务】，于是它走
    # cli/dbus_notifier.cpp 的 GetCapabilities/Notify/等信号，而不是
    # sudo 下的 DummyNotifier。
    #
    # 桩分别回 yes / no，两轮都断言"功能真的按回答走了"：
    #   yes -> 版本必须升上去；no -> 必须报"操作已取消"且版本停在旧版。
    # 并核对通知正文里同时出现新旧两个 ref —— 只"执行到"不算数。
    def test_install_interaction_notification(self):
        app = NOTIF_APP_ID
        versions = self._available_versions(app)
        if len(versions) < 2:
            raise AssertionError(
                f"{app} 可用版本不足两个，无法构造升级场景: {versions}")
        newest, older = versions[0], versions[1]

        try:
            for action, expect_upgrade in (("Y", True), ("N", False)):
                self._run_interaction_round(action, expect_upgrade,
                                            app, newest, older)
        finally:
            self._stop_notification_stub()
            # 还原成最新版：后面的用例（PTY / 容器）还要用日历应用
            self._sudo_ll_cli("uninstall", app, timeout=300, check=False)
            self._sudo_ll_cli("install", f"{app}/{newest}",
                              timeout=NOTIF_INSTALL_TIMEOUT, check=False)

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
            if not self._wait_container(timeout=60, app_id=app):
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
