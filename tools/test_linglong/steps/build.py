#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

"""玲珑冒烟测试 — 应用构建与构建器相关步骤。"""

import base64
import json
import os
import shutil
import tempfile
import random
import re
import socket
import subprocess
import sys
import threading
import time
from pathlib import Path

from ..constants import (
    DEMO_APP_ID,
    DEMO_ARCH,
    DEMO_PROJECT_DIR,
    DEMO_CUSTOM_YAML_APP_ID,
    DEMO_ARCH_YAML_APP_ID,
)


class BuildSteps:

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

        # remove 的两种参数形态：合法但不存在 vs 非法格式。
        #
        # ⚠️ 必须用【完整引用】main:id/version/arch。只给包名的话
        #    Reference::parse 的正则对不上，cmdRemoveApp 打印一行
        #    "regexp mismatched." 就 continue —— 而且【退出码仍然是 0】。
        #    实测这两种形态都返回 0，所以判据只能是输出：拿退出码
        #    当"删掉了"的证据是查不出问题的（旧用例只查"没崩溃"，
        #    结果清理语句一直是空操作，构建仓库里积了十几个探针包）。
        ref = self._builder_ref("org.deepin.nonexistent")
        r = self._ll_builder("remove", ref, check=False)
        if r.returncode != 0:
            raise AssertionError(
                f"删除不存在的应用不该失败（cmdRemoveApp 会跳过）: "
                f"rc={r.returncode} {(r.stdout + r.stderr)[:200]}")
        if "regexp mismatched" in (r.stdout + r.stderr):
            raise AssertionError(
                f"完整引用 {ref} 被当成非法格式了，说明引用拼错了")

        # 非法格式：必须明确报出格式错误。
        # 这就是上面那个坑的来源 —— 报了错但退出码还是 0。
        r = self._ll_builder("remove", "not-a-valid-ref", check=False)
        combined = r.stdout + r.stderr
        if "regexp mismatched" not in combined:
            raise AssertionError(
                f"非法引用没有报出格式错误（会静默失败）: {combined[:200]!r}")
        if "not-a-valid-ref" not in combined:
            raise AssertionError(
                f"非法引用的报错里应带上原始引用串: {combined[:200]!r}")

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

            # --skip-strip-symbols：它决定【生成的构建脚本】里是否注入 -g
            # 以及是否调用 symbols-strip.sh（linglong_builder.cpp:2042/2050）。
            # 带不带这个选项构建都能成功，所以断言 rc==0 毫无意义 ——
            # 直接检查生成出来的 entry.sh 本身。
            #
            # ⚠️ 这一步必须自己跑一次不带 --skip-run-container 的构建：
            #    buildStageBuild 在 skipRunContainer 时会在生成 entry.sh
            #    之前就 return，上面那次构建根本没产出这个文件。
            entry = Path("linglong") / "entry.sh"
            strip_markers = ("enable strip symbols",
                             "symbols-strip.sh",
                             'export CFLAGS="-g $CFLAGS"')

            r = self._ll_builder(
                "build", "--skip-fetch-source", "--skip-pull-depend",
                "--skip-commit-output", check=False)
            if r.returncode != 0:
                raise AssertionError(
                    f"ll-builder build 失败: rc={r.returncode} {r.stderr[:200]}")
            if not entry.is_file():
                raise AssertionError(f"默认构建后没有生成 {entry}")
            default_script = entry.read_text(errors="replace")
            for marker in strip_markers:
                if marker not in default_script:
                    raise AssertionError(
                        f"默认构建的 entry.sh 里应含 {marker!r}, "
                        f"实际内容: {default_script[:400]!r}")

            r = self._ll_builder(
                "build", "--skip-fetch-source", "--skip-pull-depend",
                "--skip-commit-output", "--skip-strip-symbols", check=False)
            if r.returncode != 0:
                raise AssertionError(
                    f"ll-builder build --skip-strip-symbols 失败: "
                    f"rc={r.returncode} {r.stderr[:200]}")
            if not entry.is_file():
                raise AssertionError(
                    f"--skip-strip-symbols 构建后没有生成 {entry}")
            skip_script = entry.read_text(errors="replace")
            for marker in strip_markers:
                if marker in skip_script:
                    raise AssertionError(
                        f"--skip-strip-symbols 之后 entry.sh 里不该再出现 "
                        f"{marker!r}, 实际内容: {skip_script[:400]!r}")

            # -f/--file：必须真的用指定的项目文件，而不是回落到 linglong.yaml。
            # 判据有两条，都指向"用的是哪个文件"：
            #   (1) 输出里 "Using project file <路径>" 指向我们给的那个文件
            #   (2) [Build Target] 是被我们改过的包名
            # 只改包名是为了让"读了哪个文件"在输出里可辨认 —— 两个文件除了
            # 包名完全一样，所以包名变了就只能是读了 custom 那份。
            yaml_path = Path("linglong.yaml")
            if DEMO_APP_ID not in yaml_path.read_text(errors="replace"):
                raise AssertionError(
                    f"{yaml_path} 里没有 {DEMO_APP_ID}，无法构造 -f 用例")
            custom = Path("custom-smoke.yaml")
            custom.write_text(
                yaml_path.read_text(errors="replace").replace(
                    DEMO_APP_ID, DEMO_CUSTOM_YAML_APP_ID))
            try:
                r = self._ll_builder(
                    "build", "-f", str(custom), "--skip-fetch-source",
                    "--skip-pull-depend", "--skip-commit-output", check=False)
                if r.returncode != 0:
                    raise AssertionError(
                        f"ll-builder build -f 失败: "
                        f"rc={r.returncode} {r.stderr[:200]}")
                out = r.stdout + r.stderr
                if str(custom.resolve()) not in out:
                    raise AssertionError(
                        f"build -f 的输出里没有指明使用了 {custom.resolve()}: "
                        f"{out[:400]!r}")
                if DEMO_CUSTOM_YAML_APP_ID not in out:
                    raise AssertionError(
                        f"build -f 没有采用指定项目文件里的包名 "
                        f"{DEMO_CUSTOM_YAML_APP_ID}（说明仍读了 linglong.yaml）: "
                        f"{out[:400]!r}")
            finally:
                custom.unlink(missing_ok=True)

            # 架构专属项目文件的优先级：getProjectYAMLPath 会先找
            # linglong.<arch>.yaml，找不到才回落到 linglong.yaml。
            # 判据还是"用了哪个文件"—— 包名不同就只可能读了架构专属那份。
            #
            # ⚠️ 必须放在本用例【最后】并确保删掉：这个文件一旦留下，
            #    后续所有不带 -f 的构建都会改用架构专属文件，别的用例就变了。
            arch_yaml = Path(f"linglong.{DEMO_ARCH}.yaml")
            arch_yaml.write_text(
                yaml_path.read_text(errors="replace").replace(
                    DEMO_APP_ID, DEMO_ARCH_YAML_APP_ID))
            try:
                r = self._ll_builder(
                    "build", "--skip-fetch-source", "--skip-pull-depend",
                    "--skip-commit-output", check=False)
                if r.returncode != 0:
                    raise AssertionError(
                        f"存在 {arch_yaml} 时构建失败: "
                        f"rc={r.returncode} {r.stderr[:200]}")
                out = r.stdout + r.stderr
                if str(arch_yaml.resolve()) not in out:
                    raise AssertionError(
                        f"存在 {arch_yaml} 时应优先使用它: {out[:400]!r}")
                if DEMO_ARCH_YAML_APP_ID not in out:
                    raise AssertionError(
                        f"架构专属项目文件没有生效（构建目标里没有 "
                        f"{DEMO_ARCH_YAML_APP_ID}）: {out[:400]!r}")
            finally:
                arch_yaml.unlink(missing_ok=True)

            # build COMMAND：按语义应该"进入容器执行命令而不是构建应用"，
            # 所以不能只看 rc —— 必须验证命令真的在【容器里】跑了。
            # PREFIX / TRIPLET / CWD 都是 buildStageBuild() 塞给容器进程的环境
            # 与工作目录，宿主上不存在（宿主 $PREFIX 为空、CWD 是项目目录），
            # 因此它们能证明命令是在容器内执行的，而不是被宿主直接执行了。
            # --skip-commit-output 让 build 在 build stage 之后直接成功返回。
            r = self._ll_builder(
                "build", "--skip-fetch-source", "--skip-pull-depend",
                "--skip-commit-output",
                "--", "/bin/bash", "-c",
                'echo "CMD_RAN=1"; echo "PREFIX=$PREFIX"; echo "CWD=$PWD"',
                check=False)
            if r.returncode != 0:
                raise AssertionError(
                    f"ll-builder build COMMAND 失败: rc={r.returncode} "
                    f"{(r.stdout + r.stderr)[:300]}")
            if "CMD_RAN=1" not in r.stdout:
                raise AssertionError(
                    "ll-builder build COMMAND 没有执行自定义命令, "
                    f"输出: {r.stdout[:300]!r}")
            m = re.search(r"^PREFIX=(\S*)$", r.stdout, re.M)
            if not m or not m.group(1):
                raise AssertionError(
                    "ll-builder build COMMAND 的命令不像在容器里执行: "
                    f"PREFIX 为空 (宿主上 PREFIX 本就未设置), 输出: {r.stdout[:300]!r}")
            if not re.search(r"^CWD=/project$", r.stdout, re.M):
                raise AssertionError(
                    "ll-builder build COMMAND 的工作目录应为容器内的 /project, "
                    f"实际输出: {r.stdout[:300]!r}")
        finally:
            os.chdir(old_cwd)
            self._sudo_ll_cli("uninstall", DEMO_APP_ID, check=False)

    # ── Test: --isolate-network 是否真的隔离了网络 ──
    #
    # 只断言 rc==0 是没用的：带不带这个选项构建都会成功，那只能证明代码被走到，
    # 证明不了隔离生效。这里用两条实测判据，且都做「共享 vs 隔离」对照 ——
    # 只有对照成立，才能说明差异确实是这个选项造成的：
    #   1) /proc/net/dev 与 /proc/net/route 是 netns 作用域的：
    #      共享网络时容器与宿主同 netns，能看到宿主网卡和路由；
    #      隔离后必须只剩 lo、且没有任何路由。
    #   2) 在宿主 127.0.0.1 上开一个监听端口，再从容器里连它：
    #      共享网络时能连上（这一步同时证明判据本身有效），隔离后必须连不上。
    # 判据 2 是自给自足的 —— 不依赖外网是否可达，离线 CI 上同样成立。
    def test_builder_isolate_network(self):
        listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        listener.bind(("127.0.0.1", 0))
        listener.listen(8)
        listener.settimeout(0.5)
        port = listener.getsockname()[1]
        stop = threading.Event()

        def accept_loop():
            while not stop.is_set():
                try:
                    conn, _ = listener.accept()
                except (socket.timeout, OSError):
                    continue
                try:
                    conn.close()
                except OSError:
                    pass

        worker = threading.Thread(target=accept_loop, daemon=True)
        worker.start()

        probe = (
            'echo "IFACES=$(grep -c : /proc/net/dev)"\n'
            'echo "ROUTES=$(tail -n +2 /proc/net/route | wc -l)"\n'
            'if timeout 5 bash -c "exec 3<>/dev/tcp/127.0.0.1/%d" 2>/dev/null; '
            'then echo "NET=REACHABLE"; else echo "NET=UNREACHABLE"; fi\n'
        ) % port

        def run_probe(isolate):
            args = ["build", "--skip-fetch-source", "--skip-pull-depend",
                    "--skip-commit-output"]
            if isolate:
                args.append("--isolate-network")
            args += ["--", "/bin/bash", "-c", probe]
            r = self._ll_builder(*args, timeout=600, check=False)
            if r.returncode != 0:
                raise AssertionError(
                    f"ll-builder build (isolate={isolate}) 失败: rc={r.returncode} "
                    f"{(r.stdout + r.stderr)[:300]}")
            got = {}
            for key in ("IFACES", "ROUTES", "NET"):
                m = re.search(r"^%s=(\S+)$" % key, r.stdout, re.M)
                if not m:
                    raise AssertionError(
                        f"隔离探针未输出 {key} (isolate={isolate}): "
                        f"{r.stdout[-300:]!r}")
                got[key] = m.group(1)
            return got

        old_cwd = os.getcwd()
        try:
            os.chdir(DEMO_PROJECT_DIR)
            shared = run_probe(False)
            isolated = run_probe(True)
        finally:
            os.chdir(old_cwd)
            stop.set()
            worker.join(timeout=5)
            listener.close()

        # 对照：共享网络时必须连得上，否则判据本身失效，后面的结论不成立
        if shared["NET"] != "REACHABLE":
            raise AssertionError(
                f"共享网络时容器竟然连不上宿主 127.0.0.1:{port}，"
                f"该判据无效，无法据此判断隔离: {shared}")
        if int(shared["IFACES"]) < 2 or int(shared["ROUTES"]) < 1:
            raise AssertionError(
                f"共享网络时容器应能看到宿主网卡与路由, 实际: {shared}")

        # 隔离：必须真的出不去
        if isolated["NET"] != "UNREACHABLE":
            raise AssertionError(
                f"--isolate-network 下容器仍能连上宿主 127.0.0.1:{port}，"
                f"网络并没有被隔离: {isolated}")
        if int(isolated["IFACES"]) != 1:
            raise AssertionError(
                f"--isolate-network 下容器内应只剩 lo, 实际: {isolated}")
        if int(isolated["ROUTES"]) != 0:
            raise AssertionError(
                f"--isolate-network 下容器内不应有任何路由, 实际: {isolated}")

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

            # push --module：按语义是"只推指定模块"，而不是项目里的全部模块。
            # 默认走 getProjectModule()，会依次推 binary / develop；
            # 给了 --module 就只剩那一个（main.cpp:889-893）。
            # 判据就是"到底推了哪个模块"—— handlePush 会逐个打印
            # "Pushing module: X"，这个输出直接反映被推送的模块集合。
            out = r.stdout + r.stderr
            if "Pushing module: binary" not in out:
                raise AssertionError(
                    f"不带 --module 时应推项目的全部模块（至少含 binary）: "
                    f"{out[:400]!r}")

            r = self._ll_builder("push", "--module", "develop",
                                 "--repo-url", "http://127.0.0.1:1/nope",
                                 "--repo-name", "smoke", timeout=180, check=False)
            out = r.stdout + r.stderr
            if "Pushing module: develop" not in out:
                raise AssertionError(
                    f"push --module develop 没有推 develop 模块: {out[:400]!r}")
            if "Pushing module: binary" in out:
                raise AssertionError(
                    f"push --module develop 不该再去推 binary 模块: {out[:400]!r}")
        finally:
            os.chdir(old_cwd)
            if workdir is not None:
                shutil.rmtree(workdir, ignore_errors=True)


    # ── Test: ll-builder remove --no-clean-objects ──
    #
    # cmdRemoveApp(repo, refs, prune) 里 prune 就是 !noCleanObjects：
    # 带 --no-clean-objects 时【不调用 repo.prune()】，对象仓里的东西留着。
    #
    # 判据落在对象仓的文件数上 —— 这正是这个选项的语义所在，
    # 只看 rc 是看不出来的（两种情况下 rc 都是 0）：
    #   1) 构建一个一次性的应用并提交
    #   2) remove --no-clean-objects -> 引用没了，但对象数【必须不变】
    #   3) 再 remove 一次（引用已经没了，但 prune 照跑）-> 对象数【必须减少】
    # 第 3 步是关键对照：同一批对象，只有在缺少该选项时才被清掉，
    # 所以"数量变化"确实归因于这个选项，而不是别的原因。
    #
    # ⚠️ 必须用完整引用（channel:id/version/arch）。只给包名会得到
    #    "regexp mismatched."，什么都不会删 —— 静默地变成空操作。
    def test_builder_remove_no_clean_objects(self):
        app_id = "org.deepin.cleanobjprobe"
        workdir = Path(tempfile.mkdtemp(prefix="ll-cleanobj-"))
        objects_dir = Path.home() / ".cache/linglong-builder/repo/objects"

        def count_objects() -> int:
            return sum(1 for p in objects_dir.rglob("*") if p.is_file())

        def ref_of(app: str) -> str:
            r = self._ll_builder("list", timeout=120, check=False)
            for line in r.stdout.splitlines():
                line = line.strip()
                if app in line and ":" in line:
                    return line
            return ""

        old_cwd = os.getcwd()
        ref = ""
        try:
            os.chdir(workdir)
            r = self._ll_builder("create", app_id, timeout=180, check=False)
            if r.returncode != 0:
                raise AssertionError(
                    f"ll-builder create {app_id} 失败: {r.stderr[:200]}")

            os.chdir(app_id)
            # 这里要真构建并提交（不能加 --skip-commit-output），
            # 否则对象仓里没有东西可删
            r = self._ll_builder("build", "--skip-fetch-source",
                                 "--skip-pull-depend", timeout=1800, check=False)
            if r.returncode != 0:
                raise AssertionError(
                    f"ll-builder build {app_id} 失败: "
                    f"rc={r.returncode} {(r.stdout + r.stderr)[:400]}")

            ref = ref_of(app_id)
            if not ref:
                raise AssertionError(
                    f"构建后 ll-builder list 里找不到 {app_id} 的完整引用")

            n0 = count_objects()

            r = self._ll_builder("remove", ref, "--no-clean-objects",
                                 timeout=300, check=False)
            if r.returncode != 0:
                raise AssertionError(
                    f"remove --no-clean-objects 失败: "
                    f"rc={r.returncode} {r.stderr[:200]}")
            if ref_of(app_id):
                raise AssertionError(
                    f"remove --no-clean-objects 之后 {app_id} 的引用还在，没删掉")
            n1 = count_objects()
            if n1 != n0:
                raise AssertionError(
                    f"--no-clean-objects 时不该清理对象仓: "
                    f"删前 {n0} 个对象, 删后 {n1} 个")

            # 引用已经没了，但 cmdRemoveApp 仍会走到 prune —— 用这一步做对照
            r = self._ll_builder("remove", ref, timeout=300, check=False)
            n2 = count_objects()
            if n2 >= n1:
                raise AssertionError(
                    f"不带 --no-clean-objects 时应该清理掉已无引用的对象: "
                    f"上次 {n1} 个, 这次 {n2} 个（没有减少）")
        finally:
            os.chdir(old_cwd)
            # 兜底：把引用清掉，别给后面的用例留垃圾
            if ref:
                self._ll_builder("remove", ref, timeout=300, check=False)
            shutil.rmtree(workdir, ignore_errors=True)

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
            # ll-cli uninstall 只清【包管理】那边；构建仓库里的 ref 必须用
            # ll-builder remove 清，而且要给它【完整引用】，否则静默不删，
            # 探针包会一直积在构建仓库里（实测积了十几个）。
            self._ll_builder("remove", self._builder_ref(app_id),
                             cwd=str(proj), timeout=600, check=False)
            self.flush_service_coverage()
            shutil.rmtree(proj, ignore_errors=True)

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
            # 清掉探针装出来的东西。
            # ⚠️ 两边都要清：ll-cli uninstall 清的是【包管理】那边，
            #    构建仓库里的 ref 只有 ll-builder remove 能清，
            #    而且必须给完整引用（只给包名会静默不删）。
            self._sudo_ll_cli("uninstall", "org.deepin.srcprobe",
                              timeout=300, check=False)
            self._ll_builder("remove", self._builder_ref("org.deepin.srcprobe"),
                             timeout=600, check=False)

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
            self._ll_builder("remove",
                             self._builder_ref("org.deepin.layerprobe"),
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
            self._ll_builder("remove",
                             self._builder_ref("org.deepin.layerprobe"),
                             timeout=600, check=False)
            shutil.rmtree(workdir, ignore_errors=True)
            self._sudo_ll_cli("uninstall", "org.deepin.layerprobe",
                              timeout=300, check=False)

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

                self._ll_builder("remove", self._builder_ref(app_id), cwd=str(proj),
                                 timeout=600, check=False)
        finally:
            shutil.rmtree(workdir, ignore_errors=True)

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
            self._ll_builder("remove", self._builder_ref(app_id), cwd=str(workdir),
                             timeout=600, check=False)
            shutil.rmtree(workdir, ignore_errors=True)


# ── Main ──
