#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

"""玲珑冒烟测试 — 服务管理相关步骤。"""

import os
import pty
import re
import signal
import subprocess
import time

from ..constants import CALENDAR_APP_ID, LL_CLI
from ..models import RepoState, StepResult, StepSkipped


class ServiceSteps:

    # ── 运行配置里的嵌套挂载：父子两个都必须可见 ──
    #
    # container_cfg_builder.cpp 里有一整组"挂载点树"代码完全没覆盖
    # （adjustNode 36/36、shouldFix 29/29、tryFixMountpointsTree 25/25、
    # constructMountpointsTree 22/22、insertChildRecursively 15/15、
    # generateMounts 14/14、mountBind 12/12、getRelativePath 12/12、
    # buildQuirkVolatile 8/8 …合计约 185 行）。
    #
    # 触发口不是命令行 --bind（CLI 根本没有这个选项），而是按应用的
    # 运行配置：RuntimeConfigure.mounts（见
    # libs/api/src/linglong/api/types/v1/RuntimeConfigure.hpp）。
    # 父目录和子目录各挂一个，就会走"挂载点树需要修正"的那条路。
    #
    # 断言的是功能行为：两个挂载在容器里【都必须能读到】，不是只看
    # 命令跑没跑通 —— 只挂上父的、丢了子的，就是功能不对。
    # ── TTY 透传 + 运行中信号转发 ──
    #
    # ll-init.cpp（108 行未覆盖）里两块从没走到：
    #   · delegate_run（11/40 未覆盖）—— 只在 stdin 是 tty 时才走的
    #     "把命令交给用户"路径（isatty(STDIN_FILENO)）
    #   · ChildProcess::forward_signal / reap_pending / has_zombies ——
    #     容器在跑的时候收到信号、以及子进程退出后的回收
    #
    # 断言的是功能行为：
    #   1. 从终端（pty）跑，容器里的 stdin 必须【真的是 tty】
    #      —— 交互式程序（vim、top）能不能用就看这个
    #   2. 跑着的容器被 kill 之后必须真的退出、且不留在 ps 里
    #      （信号要转发到应用进程，僵尸要被回收）
    # ── 从终端复用运行中的实例（交互路径）──
    #
    # reuseContainer 里有一整块交互 I/O 从没走到（1391-1449：把终端设成
    # raw 模式、ioctl 拿窗口大小、epoll 转发 stdin/stdout、以及一堆失败
    # 返回）。套件里现有的复用用例走的是非 pty 路径，所以碰不到。
    #
    # 断言的是功能行为（真实用户场景：一个实例在跑，另开终端往里发命令）：
    #   1. 复用必须成功，且新命令的输出必须回显到当前终端
    #   2. 复用不能偷偷再起一个容器（容器数不增加）
    # ── 客户端在任务进行中断开（Ctrl-C / 关终端）──
    #
    # package_task.cpp 里这几块整块没覆盖：PackageTask::onCallerDisconnected
    # （7/7）、PackageTaskQueue::getTask（7/7）、PackageTask::onMessage
    # （5/5）—— 套件里所有任务都是"客户端一直等到结束"。
    #
    # 断言的功能行为（真实场景：装到一半按 Ctrl-C / 关掉终端）：
    #   1. 服务端不能因此崩（服务必须还 active）
    #   2. 不能留下永久占用（后续命令必须还能正常执行，不能卡在锁上）
    #   3. 状态必须一致：应用要么装着要么没装，不能半装
    #   4. 跑完必须把应用还原成"装着"（本用例动了真实安装状态）
    def test_client_disconnect_during_task(self):
        app_id = CALENDAR_APP_ID
        row_re = re.compile(r"^(\S+)\s+\S+\s+(\d[\d.]*)\s", re.M)

        def installed():
            out = self._ll_cli("list", check=False).stdout
            return {f"{i}/{v}" for i, v in row_re.findall(out or "")}

        if not any(x.startswith(app_id + "/") for x in installed()):
            self._sudo_ll_cli("install", app_id, timeout=900, check=False)
        if not any(x.startswith(app_id + "/") for x in installed()):
            raise StepSkipped(f"{app_id} 装不上（镜像里没有？）")
        snapshot = self._installed_refs()

        # ⚠️ 必须用【会一直连着等进度】的 install：实测 ll-cli uninstall
        # 是"提交任务就返回"，客户端根本没连着，杀它不会触发
        # onCallerDisconnected（我第一版就是这么写的，0 增量）。
        # 先把应用卸掉，让 install 真的有活干，再中途把整个进程组
        # （含 sudo 包装）杀掉 —— 等价于用户按 Ctrl-C / 关终端。
        self._sudo_ll_cli("uninstall", "--force", app_id, timeout=300,
                          check=False)
        proc = subprocess.Popen(
            ["sudo", "-A", LL_CLI, "install", app_id],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
            start_new_session=True)
        try:
            time.sleep(8)  # 让任务真正跑起来再断
            try:
                os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
            except ProcessLookupError:
                pass
            proc.wait(timeout=60)
        finally:
            if proc.poll() is None:
                proc.kill()
                proc.wait()

        time.sleep(20)  # 给服务端收拾残局的时间

        active = self._run_cmd(["systemctl", "is-active",
                                "org.deepin.linglong.PackageManager.service"],
                               check=False).stdout
        if "active" not in (active or ""):
            raise AssertionError(f"客户端断开后服务不 active 了: {active!r}")

        r = self._ll_cli("list", check=False)
        if r.returncode != 0:
            raise AssertionError(
                f"客户端断开后 ll-cli list 不可用（疑似卡在锁上）: "
                f"rc={r.returncode} {(r.stderr or '')[:200]!r}")

        after = installed()
        if after != installed():
            raise AssertionError("ll-cli list 两次结果不一致（服务端状态不稳）")

        # 还原成"跑之前的确切状态"（不是"随便装回来"—— 镜像最新版
        # 可能不是原来那个版本，会静默换掉）
        self._restore_installed_refs(snapshot)
