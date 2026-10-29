#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

"""玲珑冒烟测试 — 仓库管理相关步骤（仓库重置/配置/迁移/缓存）。"""

import json
import os
import shutil
import tempfile
import re
import time
from pathlib import Path

from ..constants import (
    SMOKE_REPO_NAME,
    SMOKE_REPO_URL,
    DEMO_ARCH,
    CALENDAR_APP_ID,
)
from ..models import RepoState, StepResult, StepSkipped


class RepoSteps:

    # ── Test: 清理并重置仓库 ──
    def reset_repositories(self):
        self._sudo_ll_cli("repo", "remove", SMOKE_REPO_NAME, check=False)
        self._ll_builder("repo", "remove", SMOKE_REPO_NAME, check=False)

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

    def configure_smoke_repositories(self):
        p1 = self._next_repo_priority(self.ll_cli_state.highest_priority)
        p2 = self._next_repo_priority(self.ll_builder_state.highest_priority)
        self._sudo_ll_cli("repo", "add", SMOKE_REPO_NAME, SMOKE_REPO_URL)
        self._sudo_ll_cli("repo", "set-priority", SMOKE_REPO_NAME, str(p1))
        self._ll_builder("repo", "add", SMOKE_REPO_NAME, SMOKE_REPO_URL)
        self._ll_builder("repo", "set-priority", SMOKE_REPO_NAME, str(p2))

    def test_cache_write_failure(self):
        app_id = CALENDAR_APP_ID
        cache = "/var/lib/linglong/states.json"
        row_re = re.compile(r"^(\S+)\s+\S+\s+(\d[\d.]*)\s", re.M)

        def installed():
            out = self._ll_cli("list", check=False).stdout
            return {f"{i}/{v}" for i, v in row_re.findall(out or "")}

        def ensure_installed():
            if not any(x.startswith(app_id + "/") for x in installed()):
                self._sudo_ll_cli("install", app_id, timeout=900, check=False)

        ensure_installed()
        before = installed()
        if not any(x.startswith(app_id + "/") for x in before):
            raise StepSkipped(f"{app_id} 装不上（镜像里没有？）")

        try:
            r = self._run_cmd(["chattr", "+i", cache], sudo=True, check=False)
            if r.returncode != 0:
                raise StepSkipped("文件系统不支持 chattr +i，无法做故障注入")

            # 真实的状态变更：卸载（缓存写不进去）
            r = self._sudo_ll_cli("uninstall", "--force", app_id,
                                  timeout=300, check=False)
            if r.returncode < 0:
                raise AssertionError(
                    f"缓存不可写时卸载把进程搞崩了: rc={r.returncode}")
        finally:
            # 还原：先解除不可写，再重启服务让它重新读盘
            self._run_cmd(["chattr", "-i", cache], sudo=True, check=False)
            self._restart_package_manager()
            ensure_installed()
            after = installed()
            if before != after:
                raise AssertionError(
                    "缓存写失败 + 还原之后应用集合和开始前不一致"
                    f"（静默不一致）: {sorted(before)} -> {sorted(after)}")

    def test_repo_cache_rebuild(self):
        cache = "/var/lib/linglong/states.json"
        workdir = Path(tempfile.mkdtemp(prefix="cacheprobe-"))
        backup = workdir / "states.json"
        # ⚠️ 必须用统一的 3 段快照。原来这里用自己写的 2 段正则取 before，
        # 却拿它和 3 段的 after 相减 —— 2 段永远不等于 3 段，lost 恒等于
        # 全部 before，用例必然 FAIL（run51 实测），而且那个正则还会丢掉
        # 名称含空格的应用（org.deepin.runtime.dtk）。
        before = self._installed_refs()
        if not before:
            raise StepSkipped("本地仓库里没有已装应用，重建无从验证")

        self._run_cmd(["cp", "-a", cache, str(backup)], sudo=True,
                      check=True)
        try:
            self._run_cmd(["rm", "-f", cache], sudo=True, check=True)
            self._restart_package_manager()

            after = self._installed_refs()
            lost = sorted(before - after)
            if lost:
                raise AssertionError(
                    f"删掉缓存后这些应用从 list 里消失了（没能重建）: {lost}")

            r = self._run_cmd(["test", "-s", cache], sudo=True, check=False)
            if r.returncode != 0:
                raise AssertionError("删掉缓存后 states.json 没有被重新写出来")
        finally:
            self._run_cmd(["cp", "-a", str(backup), cache], sudo=True,
                          check=False)
            self._restart_package_manager()
            shutil.rmtree(workdir, ignore_errors=True)
            if workdir.exists():
                self._run_cmd(["rm", "-rf", str(workdir)], sudo=True,
                              check=False)

    def test_repo_config_v1_migration(self):
        cfg = "/var/lib/linglong/config.yaml"
        svc = "org.deepin.linglong.PackageManager.service"
        default_url = "https://repo-dev.cicd.getdeepin.org"
        extra = "smoke-v1-extra"

        v1 = (
            "version: 1\n"
            "defaultRepo: stable\n"
            "repos:\n"
            f"  stable: {default_url}\n"
            f"  {extra}: https://smoke.invalid/v1\n")
        bad = "this is: [not valid yaml\n  :::\n"

        def write_config(text):
            self._run_cmd(["bash", "-c", f"cat > {cfg} <<'SMOKE_EOF'\n"
                                         f"{text}SMOKE_EOF\n"],
                          sudo=True, check=True)

        def service_active():
            r = self._run_cmd(["systemctl", "is-active", svc],
                              check=False)
            return (r.stdout or "").strip() == "active"

        # 1) V1 配置：服务必须能起来，并把 map 迁移成带优先级的 list
        write_config(v1)
        self._restart_package_manager()
        if not service_active():
            raise AssertionError("V1 配置下服务没起来")
        code, out = self._pty_ll_cli("repo", "show", timeout=60, sudo=True)
        if code != 0:
            raise AssertionError(f"V1 配置下 repo show 失败: {out[:200]!r}")
        if "stable" not in out:
            raise AssertionError(f"V1 迁移后丢了默认仓库: {out[:200]!r}")
        # 默认仓库优先级 0，其余 -100 起
        prios = dict(re.findall(r"(\S+)\s+\S+\s+\S+\s+(-?\d+)", out))
        if prios.get("stable") != "0":
            raise AssertionError(
                f"V1 迁移后默认仓库优先级不是 0: {prios} / {out[:200]!r}")
        if prios.get(extra) != "-100":
            raise AssertionError(
                f"V1 迁移后第二个仓库优先级不是 -100: {prios} / {out[:200]!r}")

        # 2) 坏 YAML：服务必须拒绝启动，而不是带着半截配置跑
        write_config(bad)
        self._run_cmd(["systemctl", "reset-failed", svc], sudo=True,
                      check=False)
        self._run_cmd(["systemctl", "restart", svc], sudo=True, check=False)
        time.sleep(5)
        if service_active():
            raise AssertionError("坏 YAML 下服务居然还在跑")

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
            self._ll_builder("remove", self._builder_ref(app_id), cwd=str(workdir),
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

        # ⚠️ 走 _read_root_file 而不是 sudo cat：这个值下面会被【回写】，
        #    sudo 的 "验证成功" 杂音混进来就会把文件写成垃圾。
        original = self._read_root_file(version_file).strip()

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
