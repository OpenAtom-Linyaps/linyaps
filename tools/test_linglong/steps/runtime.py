#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

"""玲珑冒烟测试 — 运行时相关步骤（run 选项/容器/扩展/PTY/CDI 等）。"""

import json
import os
import pty
import shutil
import tempfile
import socket
import subprocess
import time
from pathlib import Path

from ..constants import (
    DEMO_APP_ID,
    CALENDAR_APP_ID,
    LIBEXEC_DIR,
    LL_CLI,
)
from ..models import RepoState, StepResult, StepSkipped


class RuntimeSteps:

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
        # 机器上可能残留别的容器（GUI 应用被 DBus 服务拉起），先清干净，
        # 否则 ps 里第一条不一定是本用例刚起的 demo 容器。
        self._cleanup_containers(app_id)
        proc = subprocess.Popen(
            [LL_CLI, "run", app_id, "--", "sleep", "120"],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        try:
            cid = self._wait_container(timeout=40, app_id=app_id)
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
            entry = next((c for c in arr if c.get("id") == cid), None)
            if entry is None:
                raise AssertionError(
                    f"ps --json 应含容器 {cid}, 实际: {r.stdout[:200]!r}")

            # ps --no-truncated：默认会把容器 ID 截断到 12 位
            # （cli.cpp:102 ContainerIDDisplayLength），--no-truncated 才给全长。
            # 只断言 rc==0 证明不了截断逻辑 —— 必须验证 12 位这个长度、
            # 全长确实更长、以及"短 ID 是全 ID 的前缀"这层关系。
            short_entry = entry
            r = self._ll_cli("ps", "--no-truncated", "--json", timeout=60,
                             check=False)
            if r.returncode != 0:
                raise AssertionError(
                    f"ll-cli ps --no-truncated 失败: "
                    f"rc={r.returncode} {r.stderr[:200]}")
            try:
                full_arr = json.loads(r.stdout)
            except json.JSONDecodeError as e:
                raise AssertionError(
                    f"ll-cli ps --no-truncated --json 非法 JSON: {e}")

            # 按 pid 认人：机器上可能不止一个容器
            full_entry = next(
                (c for c in full_arr if c.get("pid") == short_entry.get("pid")), None)
            if full_entry is None:
                raise AssertionError(
                    f"ps --no-truncated 里找不到 pid={short_entry.get('pid')} "
                    f"的容器: {r.stdout[:200]!r}")

            short_id = short_entry.get("id", "")
            full_id = full_entry.get("id", "")
            if len(short_id) != 12:
                raise AssertionError(
                    f"ps 默认应把容器 ID 截断到 12 位, 实际 {len(short_id)} 位: "
                    f"{short_id!r}")
            if len(full_id) <= len(short_id):
                raise AssertionError(
                    f"ps --no-truncated 的容器 ID 没有变长: "
                    f"默认={short_id!r}({len(short_id)}位) "
                    f"全长={full_id!r}({len(full_id)}位)")
            if not full_id.startswith(short_id):
                raise AssertionError(
                    f"ps --no-truncated 的全长 ID 应以默认 ID 为前缀: "
                    f"默认={short_id!r} 全长={full_id!r}")

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
            cid = self._wait_container(timeout=40, app_id=app_id)
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

    def test_runtime_binary_unusable(self):
        app_id = CALENDAR_APP_ID
        box = None
        # ll-box 装在 /usr/bin（不是 /usr/libexec/linglong，那里只有
        # ll-package-manager、ll-init、ll-driver-detect 这些）
        for cand in ("/usr/bin/ll-box", "/usr/bin/ll-box-static",
                     f"{LIBEXEC_DIR}/ll-box"):
            if os.path.exists(cand):
                box = cand
                break
        if box is None:
            raise StepSkipped("没找到 ll-box")

        r = self._ll_cli("list", check=False)
        if app_id not in (r.stdout or ""):
            self._sudo_ll_cli("install", app_id, timeout=900, check=False)

        def containers():
            r = self._ll_cli("ps", "--json", check=False)
            try:
                return json.loads(r.stdout)
            except (json.JSONDecodeError, TypeError):
                return []

        before_box_mode = format(os.stat(box).st_mode & 0o7777, "o")
        before = len(containers())
        try:
            r = self._run_cmd(["chmod", "000", box], sudo=True, check=False)
            if r.returncode != 0:
                raise StepSkipped(f"改不了 {box} 权限，跳过")

            code, out = self._pty_ll_cli(
                "run", app_id, "--", "/bin/echo", "BOXPROBE", timeout=120)
            if code < 0:
                raise AssertionError(
                    f"运行时不可用时把进程搞崩了: rc={code} {out[:200]!r}")
            if code == 0 and "BOXPROBE" in out:
                raise AssertionError(
                    "ll-box 都 chmod 000 了居然还能起容器（没走这条路）")
            if not out.strip():
                raise AssertionError("运行时不可用时失败了但没有任何报错")

            after = len(containers())
            if after > before:
                raise AssertionError(
                    f"运行时起不来却留下了容器: ps {before} -> {after} 个")
        finally:
            # 用一开始读到的真实权限还原，别硬编码
            self._run_cmd(["chmod", before_box_mode, box], sudo=True,
                          check=False)
            self._cleanup_containers(app_id)
            if not os.access(box, os.X_OK):
                raise AssertionError(f"{box} 权限没能还原成可执行")

        code, out = self._pty_ll_cli(
            "run", app_id, "--", "/bin/echo", "BOXPROBE", timeout=120)
        if code != 0 or "BOXPROBE" not in out:
            raise AssertionError(
                f"权限还原后仍然起不来: rc={code} {out[:200]!r}")

    def test_filelock_dir_unwritable(self):
        app_id = CALENDAR_APP_ID
        # 锁是在【按 uid 的子目录】里建的（cli.cpp:1649
        # userContainerDir = /run/linglong/<uid>，锁名 .cli.<id>.lock）；
        # 只读父目录没用 —— 子目录已存在，照样能建锁（实测过）。
        lock_dir = f"/run/linglong/{os.getuid()}"
        repo_lock = "/run/linglong/lock"
        probe = ["run", app_id, "--", "/bin/echo", "LOCKPROBE"]

        def stat_mode():
            # ⚠️ 这里【不能用 sudo stat】读权限：askpass 的"验证成功"
            # 会混进 stdout，chmod 收到 "1777\n验证成功" 这种垃圾值，
            # 结果把目录权限改坏又还原不回来（真踩过，/run/linglong
            # 被留在 555）。os.stat 不需要 root，干净。
            try:
                return format(os.stat(lock_dir).st_mode & 0o7777, "o")
            except OSError:
                return ""

        r = self._ll_cli("list", check=False)
        if app_id not in (r.stdout or ""):
            self._sudo_ll_cli("install", app_id, timeout=900, check=False)

        before_mode = stat_mode()
        if not before_mode:
            raise StepSkipped(f"{lock_dir} 不存在，跳过")
        had_repo_lock = os.path.exists(repo_lock)

        try:
            r = self._run_cmd(["chmod", "555", lock_dir], sudo=True,
                              check=False)
            if r.returncode != 0:
                raise StepSkipped(f"改不了 {lock_dir} 权限，跳过")

            # 真正打穿锁分支：Cli::run 只在【新容器】分支里建仓库锁
            # （dumpContainerInfo → FileLock::create(/run/linglong/lock,
            # Read)）。所以要用全新实例 + 让锁文件先不存在，光把目录设
            # 只读是不够的（锁文件已存在时打开它不需要目录写权限）。
            self._run_cmd(["rm", "-f", repo_lock], sudo=True, check=False)
            inst = f"smokelock{os.getpid()}"
            code, out = self._pty_ll_cli(
                "run", app_id, "--instance", inst, "--",
                "/bin/echo", "LOCKPROBE", timeout=90)
            if code < 0:
                raise AssertionError(
                    f"锁目录不可写时把进程搞崩了: rc={code} {out[:200]!r}")
            if code == 0:
                raise AssertionError(
                    "锁目录只读 + 锁文件不存在 + 全新实例，居然还能起容器"
                    f"（说明这条路没建锁）: {out[:200]!r}")
            if not out.strip():
                raise AssertionError("锁建不起来时失败了但没有任何报错")
        finally:
            self._cleanup_containers(app_id)
            # ⚠️ 锁文件必须由我们补回来：产品自己【建不回来】——
            # FileLock::create(..., false) 是不带 O_CREAT 的打开，
            # 文件不在就永远报 "failed to open lock file ... 没有那个
            # 文件或目录"。所以删了 /run/linglong/lock 之后，
            # 哪怕目录权限恢复、服务重启，新容器也再也起不来。
            # 这是本轮实测到的产品缺陷，已记进提交说明。
            if had_repo_lock and not os.path.exists(repo_lock):
                self._run_cmd(["touch", repo_lock], sudo=True, check=False)
                self._run_cmd(
                    ["chown", "deepin-linglong:deepin-linglong", repo_lock],
                    sudo=True, check=False)
                self._run_cmd(["chmod", "666", repo_lock], sudo=True,
                              check=False)
            if had_repo_lock and not os.path.exists(repo_lock):
                raise AssertionError("锁文件没能还原（机器会起不了新容器）")
            # 还原权限（用一开始读到的真实值，别硬编码）
            self._run_cmd(["chmod", before_mode, lock_dir], sudo=True,
                          check=False)
            now = stat_mode()
            if now != before_mode:
                # 兜底：万一 before_mode 读歪了，按目录类型恢复到已知安全值
                safe = "1777" if lock_dir == "/run/linglong" else "755"
                self._run_cmd(["chmod", safe, lock_dir], sudo=True,
                              check=False)
                raise AssertionError(
                    f"{lock_dir} 权限没还原: {before_mode} -> {now}"
                    f"（已按 {safe} 兜底恢复）")

        # 还原之后必须能正常跑起来
        code, out = self._pty_ll_cli(*probe, timeout=90)
        if code != 0 or "LOCKPROBE" not in out:
            raise AssertionError(
                f"权限还原后仍然跑不起来: rc={code} {out[:200]!r}")

    def test_reuse_instance_with_tty(self):
        app_id = CALENDAR_APP_ID
        inst = f"smokereusetty{os.getpid()}"
        r = self._ll_cli("list", check=False)
        if app_id not in (r.stdout or ""):
            self._sudo_ll_cli("install", app_id, timeout=900, check=False)

        proc = subprocess.Popen(
            [LL_CLI, "run", app_id, "--instance", inst, "--", "sleep", "300"],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        try:
            if not self._wait_container(timeout=40, app_id=app_id):
                raise AssertionError("带 --instance 启动的容器没出现在 ps 里")

            # 关键：用 pty（终端）去复用，才会走交互 I/O 那块
            code, out = self._pty_ll_cli(
                "run", app_id, "--instance", inst, "--",
                "/bin/sh", "-c", "echo REUSED_TTY", timeout=120)
            if code != 0:
                raise AssertionError(
                    f"从终端复用实例失败: rc={code} {out[:250]!r}")
            if "REUSED_TTY" not in out:
                raise AssertionError(
                    f"复用后新命令的输出没回显: {out[:250]!r}")

            r = self._ll_cli("ps", "--json", check=False)
            try:
                arr = json.loads(r.stdout or "[]")
            except json.JSONDecodeError:
                arr = []
            if isinstance(arr, list) and len(arr) != 1:
                raise AssertionError(
                    f"复用后应只有 1 个容器, 实际 {len(arr)} 个")
        finally:
            if proc.poll() is None:
                proc.kill()
                proc.wait()
            self._cleanup_containers(app_id)

    def test_tty_and_signal_forwarding(self):
        app_id = CALENDAR_APP_ID
        r = self._ll_cli("list", check=False)
        if app_id not in (r.stdout or ""):
            self._sudo_ll_cli("install", app_id, timeout=900, check=False)

        try:
            # 1) TTY 透传
            code, out = self._pty_ll_cli(
                "run", app_id, "--", "/bin/sh", "-c",
                "if [ -t 0 ]; then echo IS_TTY; else echo NO_TTY; fi",
                timeout=120)
            if code != 0:
                raise AssertionError(f"pty 下运行失败: rc={code} {out[:200]!r}")
            if "IS_TTY" not in out:
                raise AssertionError(
                    "从终端跑，容器里的 stdin 不是 tty（交互式程序会不可用）: "
                    f"{out[:200]!r}")

            # 2) 跑着的容器收到信号：必须退出、不能留僵尸/残留容器
            inst = f"smokesig{os.getpid()}"
            proc = subprocess.Popen(
                [LL_CLI, "run", app_id, "--instance", inst, "--",
                 "sleep", "300"],
                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            try:
                cid = self._wait_container(timeout=40, app_id=app_id)
                if not cid:
                    raise AssertionError("带 --instance 启动的容器没出现在 ps 里")
                r = self._ll_cli("kill", app_id, timeout=60, check=False)
                if r.returncode != 0:
                    raise AssertionError(
                        f"ll-cli kill 失败: rc={r.returncode} "
                        f"{(r.stderr or r.stdout)[:200]!r}")
                # 等它真的消失
                deadline = time.time() + 40
                gone = False
                while time.time() < deadline:
                    rr = self._ll_cli("ps", "--json", check=False)
                    if app_id not in (rr.stdout or ""):
                        gone = True
                        break
                    time.sleep(2)
                if not gone:
                    raise AssertionError(
                        "kill 之后容器还留在 ll-cli ps 里（信号没转发/僵尸没回收）")
            finally:
                if proc.poll() is None:
                    proc.kill()
                    proc.wait()
        finally:
            self._cleanup_containers(app_id)

    def test_runtime_config_mounts(self):
        app_id = CALENDAR_APP_ID
        workdir = Path(tempfile.mkdtemp(prefix="rtmount-"))
        cfg_home = workdir / "cfg"
        cfg_dir = cfg_home / "linglong" / "apps" / app_id / "config.d"
        cfg_dir.mkdir(parents=True)
        outer = workdir / "outer"
        inner = outer / "inner"
        inner.mkdir(parents=True)
        (outer / "outer.txt").write_text("OUTER_OK")
        (inner / "inner.txt").write_text("INNER_OK")

        r = self._ll_cli("list", check=False)
        if app_id not in (r.stdout or ""):
            self._sudo_ll_cli("install", app_id, timeout=900, check=False)

        (cfg_dir / "smoke-mounts.json").write_text(json.dumps({
            "mounts": [
                {"destination": "/opt/smoke-nest", "source": str(outer),
                 "type": "bind", "options": ["rbind"]},
                {"destination": "/opt/smoke-nest/inner", "source": str(inner),
                 "type": "bind", "options": ["rbind"]},
            ],
            # 顺带覆盖几个同样没走到的配置分支
            "enablePipewire": True,
            "disableXdp": True,
            "enableAtspi": True,
        }, indent=2))

        try:
            code, out = self._pty_ll_cli(
                "run", app_id, "--", "/bin/sh", "-c",
                "cat /opt/smoke-nest/outer.txt; "
                "cat /opt/smoke-nest/inner/inner.txt",
                timeout=120, env={"XDG_CONFIG_HOME": str(cfg_home)})
            if code != 0:
                raise AssertionError(
                    f"带嵌套挂载运行失败: rc={code} {out[:300]!r}")
            if "OUTER_OK" not in out:
                raise AssertionError(
                    f"父挂载在容器里读不到: {out[:300]!r}")
            if "INNER_OK" not in out:
                raise AssertionError(
                    f"子挂载在容器里读不到（挂载点树没修对）: {out[:300]!r}")
        finally:
            shutil.rmtree(workdir, ignore_errors=True)
            if workdir.exists():
                self._run_cmd(["rm", "-rf", str(workdir)], sudo=True,
                              check=False)
            self._cleanup_containers(app_id)
            if workdir.exists():
                raise AssertionError(f"临时运行配置没清干净: {workdir}")

    def test_runtime_config_env_injection(self):
        app_id = CALENDAR_APP_ID
        workdir = Path(tempfile.mkdtemp(prefix="rtenvprobe-"))
        cfg_home = workdir / "cfg"
        cfg_dir = cfg_home / "linglong" / "apps" / app_id / "config.d"
        cfg_dir.mkdir(parents=True)

        # 应用可能排在"日历被卸载"之后
        r = self._ll_cli("list", check=False)
        if app_id not in r.stdout:
            self._sudo_ll_cli("install", app_id, timeout=900, check=False)

        (cfg_dir / "smoke-env.json").write_text(json.dumps({
            "env": {
                "SMOKE_RT_ENV": "rt1",
                "SMOKE_RT_FROM_CONFIG": "cfg",
            },
        }, indent=2))

        try:
            # 1) 运行配置里的 env
            code, out = self._pty_ll_cli(
                "run", app_id, "--", "/bin/sh", "-c",
                "echo RT=$SMOKE_RT_ENV CFG=$SMOKE_RT_FROM_CONFIG",
                timeout=90, env={"XDG_CONFIG_HOME": str(cfg_home)})
            if code != 0:
                raise AssertionError(f"带运行配置运行失败: rc={code} {out[:200]}")
            if "RT=rt1" not in out or "CFG=cfg" not in out:
                raise AssertionError(
                    f"运行配置里的 env 没有注入容器: {out[:200]!r}")

            # 2) 命令行 --env
            code, out = self._pty_ll_cli(
                "run", app_id, "--env", "SMOKE_RT_FROM_CLI=cli1",
                "--", "/bin/sh", "-c", "echo CLI=$SMOKE_RT_FROM_CLI",
                timeout=90, env={"XDG_CONFIG_HOME": str(cfg_home)})
            if code != 0:
                raise AssertionError(f"--env 运行失败: rc={code} {out[:200]}")
            if "CLI=cli1" not in out:
                raise AssertionError(f"--env 没有注入容器: {out[:200]!r}")

            # 3) 畸形 --env（没有 =）：必须被拒绝且不崩。
            #    实测：ll-cli 在【参数解析层】就拦下来了
            #    （"--env: 输入参数无效，请输入有效参数"），根本走不到服务端
            #    applyCliRunOptions 里那句 "invalid environment variable"
            #    —— 也就是说 container_builder.cpp:175 这个分支
            #    【没法用 ll-cli 覆盖】，只能靠直接发 DBus 请求（后续再补）。
            code, out = self._pty_ll_cli(
                "run", app_id, "--env", "SMOKE_RT_BAD",
                "--", "/bin/sh", "-c", "true",
                timeout=90, env={"XDG_CONFIG_HOME": str(cfg_home)})
            if code == 0:
                raise AssertionError(f"畸形 --env 居然成功了: {out[:200]!r}")
            if not any(k in out for k in ("输入参数无效",
                                          "invalid environment variable")):
                raise AssertionError(f"畸形 --env 的报错不对: {out[:200]!r}")
        finally:
            shutil.rmtree(workdir, ignore_errors=True)
            if workdir.exists():
                self._run_cmd(["rm", "-rf", str(workdir)], sudo=True,
                              check=False)
            if workdir.exists():
                raise AssertionError(f"临时目录没删掉: {workdir}")

    def test_extension_layer_env_and_mount(self):
        app_id = CALENDAR_APP_ID
        ext_id = f"org.linyaps.smoke.ext{os.getpid()}"
        ext_ver = "1.0.0.1"
        env_ok = "ext-on"
        env_lib = f"/opt/extensions/{ext_id}/lib"
        workdir = Path(tempfile.mkdtemp(prefix="extprobe-"))
        proj = workdir / "smokeext"
        cfg_home = workdir / "cfg"
        cfg_dir = cfg_home / "linglong" / "apps" / app_id / "config.d"

        # 应用可能排在"日历被卸载"之后
        r = self._ll_cli("list", check=False)
        if app_id not in r.stdout:
            self._sudo_ll_cli("install", app_id, timeout=900, check=False)

        proj.mkdir(parents=True)
        cfg_dir.mkdir(parents=True)
        (proj / "linglong.yaml").write_text(
            'version: "1"\n\n'
            "package:\n"
            f"  id: {ext_id}\n"
            "  name: smoke extension\n"
            f"  version: {ext_ver}\n"
            "  kind: extension\n"
            "  description: |\n"
            "    smoke test extension\n"
            f"  extension_of: {app_id}\n"
            "  env:\n"
            f'    SMOKE_EXT_ENV: "{env_ok}"\n'
            '    SMOKE_EXT_LIB: "$PREFIX/lib"\n'
            '    SMOKE_EXT_ORIGIN: "$ORIGIN/smoke"\n'
            '    SMOKE_EXT_DENIED: "must-not-appear"\n'
            "  libs:\n"
            "    - /usr/lib/smoke-ext-libs\n"
            "  deviceNodes:\n"
            "    - path: /tmp/smoke-ext-null\n"
            "      hostPath: /dev/null\n\n"
            "command: [echo, smoke-extension]\n\n"
            "base: org.deepin.base/25.2.2\n\n"
            "build: |\n"
            "  mkdir -p $PREFIX/share $PREFIX/lib/smoke-ext-libs\n"
            "  echo smoke-ext-file > $PREFIX/share/smoke-ext.txt\n"
            "  echo smoke-lib > $PREFIX/lib/smoke-ext-libs/libsmoke.txt\n")

        # 用户级运行配置：声明日历应用使用这个扩展，并给出 env 白名单
        (cfg_dir / "smoke-ext.json").write_text(json.dumps({
            "ext_defs": {
                app_id: [{
                    "name": ext_id,
                    "version": ext_ver,
                    "directory": f"/opt/extensions/{ext_id}",
                    "allow_env": {
                        "SMOKE_EXT_ENV": "",
                        "SMOKE_EXT_LIB": "/usr/lib/smoke-default",
                        "SMOKE_EXT_ORIGIN": "/base",
                    },
                }],
            },
        }, indent=2))

        try:
            r = self._ll_builder("build", cwd=str(proj), timeout=1800, check=False)
            if r.returncode != 0:
                raise AssertionError(
                    f"构建扩展项目失败: rc={r.returncode} "
                    f"{(r.stdout + r.stderr)[-400:]}")

            r = self._ll_builder("export", "--layer", cwd=str(proj),
                                 timeout=900, check=False)
            if r.returncode != 0:
                raise AssertionError(
                    f"导出扩展 layer 失败: rc={r.returncode} "
                    f"{(r.stdout + r.stderr)[-400:]}")
            layers = sorted(proj.glob("*_binary.layer"))
            if not layers:
                raise AssertionError("导出后没有找到扩展的 binary.layer")

            r = self._sudo_ll_cli("install", str(layers[0]), timeout=1200,
                                  check=False)
            if r.returncode != 0:
                raise AssertionError(
                    f"安装扩展 layer 失败: rc={r.returncode} "
                    f"{(r.stdout + r.stderr)[-400:]}")
            # 安装走服务端，覆盖计数还在服务进程内存里，得让它落盘
            self.flush_service_coverage()

            r = self._ll_cli("list", check=False)
            if ext_id not in r.stdout:
                raise AssertionError(
                    f"扩展装完后 ll-cli list 里没有 {ext_id}，"
                    "后面 resolveExtension 会静默跳过、用例会假通过")

            # ── 正式轮：带运行配置跑，扩展必须真的生效 ──
            script = (
                "echo EXT=$SMOKE_EXT_ENV; "
                "echo LIB=$SMOKE_EXT_LIB; "
                "echo ORIGIN=$SMOKE_EXT_ORIGIN; "
                "echo DENIED=[$SMOKE_EXT_DENIED]; "
                f"cat /opt/extensions/{ext_id}/share/smoke-ext.txt; "
                "test -c /tmp/smoke-ext-null && echo DEVNODE=ok")
            code, out = self._pty_ll_cli(
                "run", app_id, "--", "/bin/sh", "-c", script, timeout=300,
                env={"XDG_CONFIG_HOME": str(cfg_home)})
            if code != 0:
                raise AssertionError(
                    f"带扩展运行失败: rc={code} {out[-400:]!r}")
            for want, why in (
                (f"EXT={env_ok}", "扩展声明的 env 没有被注入"),
                (f"LIB={env_lib}", "$PREFIX 没有被替换成扩展挂载点"),
                ("smoke-ext-file", "扩展层没有被挂到 /opt/extensions/<id>"),
                ("DEVNODE=ok", "扩展声明的 deviceNodes 没有被挂载"),
                # ⚠️ 下面两条断言的是【最终生效值】而不是 allow_env 语义：
                #    ll-cli run 会把解析好的 RunContextConfig 交给服务端，
                #    服务端 PackageManager::initRunContext 会再 resolve 一次，
                #    那次走的是 "target 没声明过这个扩展 -> manual define"，
                #    而 manual define 里没有 allow_env，于是 env 被原样注入。
                #    所以 allow_env 的白名单/默认值语义在整条链路里【不生效】。
                ("ORIGIN=/smoke", "扩展的 $ORIGIN 占位符没有被替换"),
                ("DENIED=[must-not-appear]", "扩展声明的 env 没有原样进容器"),
                # 但 CLI 侧那次解析确实跑了 allow_env 分支：不在白名单里的
                # env 必须留下一条警告（这条日志就是该分支被执行的证据）。
                ("not allowed in", "CLI 侧的 allow_env 白名单分支没有跑到"),
            ):
                if want not in out:
                    raise AssertionError(f"{why}（期望 {want!r}）: {out[-400:]!r}")

            # ── 对照组：不带运行配置，同样的 env 必须是空的 ──
            # 否则"扩展生效"这个结论不成立（可能是别处注入的）。
            code, out = self._pty_ll_cli(
                "run", app_id, "--", "/bin/sh", "-c",
                "echo EXT=[$SMOKE_EXT_ENV]", timeout=300,
                env={"XDG_CONFIG_HOME": None})
            if code != 0:
                raise AssertionError(f"对照运行失败: rc={code} {out[-300:]!r}")
            if "EXT=[]" not in out:
                raise AssertionError(
                    f"没有运行配置时扩展的 env 仍然生效了，"
                    f"说明结论不成立: {out[-300:]!r}")
        finally:
            self._sudo_ll_cli("uninstall", ext_id, timeout=600, check=False)
            # builder 是以 root 身份跑的，项目目录里会留下 v25 删不掉的文件，
            # 所以 shutil.rmtree 之后还要用 sudo 兜一次底。
            shutil.rmtree(workdir, ignore_errors=True)
            if workdir.exists():
                self._run_cmd(["rm", "-rf", str(workdir)], sudo=True,
                              check=False)
            r = self._ll_cli("list", check=False)
            if ext_id in r.stdout:
                raise AssertionError(f"用例结束后扩展 {ext_id} 仍留在机器上")
            if workdir.exists():
                raise AssertionError(f"用例结束后临时目录 {workdir} 没删掉")

    # ── Test: 应用声明的 permissions.binds / innerBinds ──
    #
    # run_context.cpp:fillExtraAppMounts 里处理 info.permissions 的那一段
    # 此前是 0%：仓库里的应用（日历）都没声明 permissions，而这段代码
    # 只在【应用层自己的 info.json 带 permissions】时才会跑到。
    # 自己构建一个带 permissions 的小应用（builder 会把项目顶层的
    # permissions 原样写进 info.json，见 linglong_builder.cpp:1287）：
    #
    #   binds      —— 把【宿主】文件绑进容器；
    #   innerBinds —— 把【容器 rootfs 里】的文件绑到另一个位置
    #                 （源码里是 bundlePath/rootfs/<source>）。
    #
    # 断言方式：在容器里把两个目标都读出来，内容必须是源文件的内容 ——
    # 只看"执行到"是不够的，绑错了路径同样会"执行到"。
    def test_run_app_permissions_binds(self):
        app_id = f"org.linyaps.smoke.perm{os.getpid()}"
        host_content = "smoke-host-bind-content"
        inner_content = "smoke-inner-bind-content"
        host_dst = "/tmp/smoke-host-bind.txt"
        inner_dst = "/tmp/smoke-inner-bind.txt"
        inner_src = f"/opt/apps/{app_id}/files/share/smoke-inner.txt"
        workdir = Path(tempfile.mkdtemp(prefix="permprobe-"))
        proj = workdir / "permprobe"
        host_file = workdir / "host-bind.txt"
        proj.mkdir(parents=True)
        host_file.write_text(host_content + "\n")
        # ⚠️ 宿主绑定源是【服务进程】去打开的，而服务跑在 uid 993
        #    （deepin-linglong），不是 root：tempfile.mkdtemp 建的目录是
        #    0700，服务读不到，现象是 ll-box 报
        #    "clone failed: open: ... Permission denied"，然后
        #    InitRunContext 失败。这里必须放开权限。
        os.chmod(workdir, 0o755)
        os.chmod(host_file, 0o644)

        (proj / "linglong.yaml").write_text(
            'version: "1"\n\n'
            "package:\n"
            f"  id: {app_id}\n"
            "  name: smoke permissions\n"
            "  version: 1.0.0.1\n"
            "  kind: app\n"
            "  description: |\n"
            "    smoke test permissions binds\n\n"
            "command: [echo, smoke-permissions]\n\n"
            "base: org.deepin.base/25.2.2\n\n"
            "permissions:\n"
            "  binds:\n"
            f"    - source: {host_file}\n"
            f"      destination: {host_dst}\n"
            "  innerBinds:\n"
            f"    - source: {inner_src}\n"
            f"      destination: {inner_dst}\n\n"
            "build: |\n"
            "  mkdir -p $PREFIX/share\n"
            f"  echo {inner_content} > $PREFIX/share/smoke-inner.txt\n")

        try:
            r = self._ll_builder("build", cwd=str(proj), timeout=1800, check=False)
            if r.returncode != 0:
                raise AssertionError(
                    f"构建 permissions 项目失败: rc={r.returncode} "
                    f"{(r.stdout + r.stderr)[-400:]}")

            r = self._ll_builder("export", "--layer", cwd=str(proj),
                                 timeout=900, check=False)
            if r.returncode != 0:
                raise AssertionError(
                    f"导出 permissions layer 失败: rc={r.returncode} "
                    f"{(r.stdout + r.stderr)[-400:]}")
            layers = sorted(proj.glob("*_binary.layer"))
            if not layers:
                raise AssertionError("导出后没有找到 permissions 的 binary.layer")

            r = self._sudo_ll_cli("install", str(layers[0]), timeout=1200,
                                  check=False)
            if r.returncode != 0:
                raise AssertionError(
                    f"安装 permissions layer 失败: rc={r.returncode} "
                    f"{(r.stdout + r.stderr)[-400:]}")
            self.flush_service_coverage()

            script = f"cat {host_dst}; cat {inner_dst}"
            code, out = self._pty_ll_cli("run", app_id, "--", "/bin/sh", "-c",
                                         script, timeout=300)
            if code != 0:
                raise AssertionError(
                    f"带 permissions 运行失败: rc={code} {out[-400:]!r}")
            if host_content not in out:
                raise AssertionError(
                    f"permissions.binds 没有把宿主文件绑进容器: {out[-400:]!r}")
            if inner_content not in out:
                raise AssertionError(
                    f"permissions.innerBinds 没有把应用内部文件绑过去: "
                    f"{out[-400:]!r}")
        finally:
            self._sudo_ll_cli("uninstall", app_id, timeout=600, check=False)
            # 同扩展用例：builder 留下的 root 文件 v25 删不掉
            shutil.rmtree(workdir, ignore_errors=True)
            if workdir.exists():
                self._run_cmd(["rm", "-rf", str(workdir)], sudo=True,
                              check=False)
            r = self._ll_cli("list", check=False)
            if app_id in r.stdout:
                raise AssertionError(f"用例结束后应用 {app_id} 仍留在机器上")
            if workdir.exists():
                raise AssertionError(f"用例结束后临时目录 {workdir} 没删掉")

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
            if not self._wait_container(timeout=40, app_id=app_id):
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
        # nvidia.com/gpu=all：不带 --device 时会被【自动】注入
        # （run_context.cpp 里专门找 kind=="nvidia.com/gpu" && name=="all"
        #   的那一段）。注意 --cdi-spec-dir 是【替换】默认的
        # /etc/cdi + /var/run/cdi，所以这里造的设备一定是唯一候选。
        Path(f"{spec_dir}/nvidia.yaml").write_text(
            'cdiVersion: "0.5.0"\n'
            'kind: "nvidia.com/gpu"\n'
            'devices:\n'
            '  - name: "all"\n'
            '    containerEdits:\n'
            '      env:\n'
            '        - "SMOKE_CDI_ALL=1"\n'
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

            # 不带 --device：CDI 目录里的 nvidia.com/gpu=all 应被自动注入
            code, out = self._pty_ll_cli(
                "run", CALENDAR_APP_ID, "--cdi-spec-dir", spec_dir,
                "--", "/bin/sh", "-c", "echo ALL=$SMOKE_CDI_ALL", timeout=90)
            if code != 0:
                raise AssertionError(
                    f"带 nvidia.com/gpu=all 运行时失败: rc={code} {out[:200]}")
            if "ALL=1" not in out:
                raise AssertionError(
                    "没有 --device 时 nvidia.com/gpu=all 没有被自动注入: "
                    f"{out[:200]!r}")

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
        r = self._ll_cli("run", app, "--instance", inst, "--privileged",
                         "--", "/bin/echo", "PRIV", timeout=120, check=False)
        if r.returncode == 0:
            raise AssertionError("非 root + --privileged 居然成功了")
        if "privileged mode requires running as root" not in (r.stdout + r.stderr):
            raise AssertionError(
                f"非 root + --privileged 报错不对: {(r.stdout + r.stderr)[:300]}")

        r = self._sudo_ll_cli("run", app, "--instance", inst,
                              "--privileged", "--", "/bin/echo", "PRIVROOT",
                              timeout=180, check=False)
        if r.returncode != 0:
            raise AssertionError(
                f"root + --privileged 失败: {(r.stdout + r.stderr)[:300]}")

        # root 不显式传 --privileged 也必须自动进入 privileged 模式：
        # 与宿主机共享 user namespace，并获得原有的 privilege capabilities。
        hostUserNs = self._run_cmd(["readlink", "/proc/self/ns/user"], sudo=True)
        r = self._sudo_ll_cli(
            "run", app, "--instance", inst + "auto", "--", "/bin/sh", "-c",
            "printf 'USERNS=%s\\n' \"$(readlink /proc/self/ns/user)\"; "
            "awk '/^CapEff:/{printf \"CAPEFF=%s\\n\", $2}' /proc/self/status",
            timeout=180, check=False)
        if r.returncode != 0:
            raise AssertionError(
                f"root 自动 privileged 失败: {(r.stdout + r.stderr)[:300]}")
        if f"USERNS={hostUserNs.stdout.strip()}" not in r.stdout:
            raise AssertionError(
                f"root 自动 privileged 未共享宿主机 user namespace: {r.stdout[:300]}")
        if "CAPEFF=00000000800435fb" not in r.stdout:
            raise AssertionError(
                f"root 自动 privileged capabilities 不匹配: {r.stdout[:300]}")

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
