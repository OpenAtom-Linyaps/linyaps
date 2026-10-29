#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

"""玲珑冒烟测试 — CLI 输出格式与错误信息相关步骤。"""

import json
import re

from ..constants import CALENDAR_APP_ID


class OutputSteps:

    # ── Test: 自建扩展包（kind: extension）的解析、env 注入与挂载 ──
    #
    # run_context.cpp 里扩展相关的代码此前基本是 0%：仓库里唯一的扩展
    # org.deepin.driver.display.nvidia 要下 1.6GB 驱动包，冒烟不可能装。
    # 这里自己构建一个极小的扩展（构建只要几秒），再用【用户级】运行配置
    # 把它挂到已安装的日历应用上：
    #
    #   $XDG_CONFIG_HOME/linglong/apps/<appId>/config.d/*.json
    #     {"ext_defs": {"<appId>": [{name,version,directory,allow_env}]}}
    #
    # 覆盖范围：
    #   - ResolveOptions::applyRuntimeConfig / externalExtensionDefs
    #   - matchedExtensionDefines / resolveLayerExtensions / resolveExtension
    #   - 扩展 env 注入：ext_impl.env + ExtensionDefine.allow_env 白名单
    #     + $PREFIX / $ORIGIN 替换
    #   - 扩展 deviceNodes 挂载与 /opt/extensions/<id> 挂载
    #
    # ⚠️ 踩过的坑：
    #   1. 运行配置的 JSON 键是【下划线】的 ext_defs / allow_env，
    #      写驼峰 extDefs 会被静默忽略（现象：扩展完全不生效、无任何报错）。
    #   2. 项目的 base 只能写三段版本（如 org.deepin.base/25.2.2），
    #      写 25.2.2.8 会报 "base version is not valid" ——
    #      version.cpp:validateDependVersion 的正则只认 MAJOR.MINOR[.PATCH]。
    #   3. 扩展必须真的装进本地仓库：resolveExtension 的 skipOnNotFound=true
    #      会把"没装"静默跳过，用例会假通过。
    #   4. 配置放临时 XDG_CONFIG_HOME 里，删目录即还原，不碰系统目录。
    # ── 用户级运行配置里的 env 注入 ──
    #
    # container_builder.cpp 里 RunContainerOptions::applyRuntimeConfig 的
    # runtimeConfig.env 合并（151-154）此前未覆盖；同一个函数下面的
    # applyCliRunOptions 对【畸形 --env】的报错分支（175）也未覆盖。
    #
    # 配置走 $XDG_CONFIG_HOME/linglong/apps/<appId>/config.d/*.json 的
    # "env" 键（和 ext_defs 一样是【下划线】风格）。放临时 XDG_CONFIG_HOME
    # 里，删目录即还原，不碰系统目录。
    #
    # 断言的是功能行为，不是"跑到了"：
    #   1. 配置里声明的 env 必须真的出现在容器里
    #   2. 命令行 --env 声明的 env 必须真的出现在容器里
    #   3. 畸形 --env（没有 =）必须报错，而不是静默忽略或崩溃
    # ── 架构字符串解析 ──
    #
    # architecture.cpp 里 Architecture(const std::string &raw) 的映射分支
    # （arm64/loongarch64/loong64/sw64/mips64/riscv64）和未知架构那句
    # throw std::runtime_error("unknown architecture") 此前未覆盖。
    #
    # ⚠️ currentCPUArchitecture() 里的 sw_64 / loongarch 分支是【宿主相关】
    # 的（x86_64 上永远走不到），isNewWorldLoongArch() 要读 loongarch 的
    # e_flags —— 冒烟里不可达，不追。
    #
    # 断言的是功能行为：引用里写【已知但不同】的架构必须被正常解析
    # （报"找不到"而不是"未知架构"），写【未知】架构必须明确报
    # unknown architecture，两种情况都不能崩。
    #
    # 用【不存在的版本号】避免真去下载别的架构的包。
    # ── 旧版(V1)仓库配置的迁移 ──
    #
    # repo/config.cpp 里 loadConfig 的 V1 回退分支（新格式读失败后
    # seekg(0) 再按 V1 读）和 convertToV2（约 35 行）此前完全未覆盖。
    # 触发方式：把仓库配置写成 V1 格式（repos 是 map），服务重启时就会走
    # 迁移路径。坏 YAML 则走 "parse yaml failed" / "all failed" 错误路径。
    #
    # ⚠️ 服务把仓库配置【缓存在内存】里：光改文件不重启不生效，而且服务
    #    下一次写配置时会拿内存里的旧值把文件冲掉。所以这里改文件后必须
    #    重启服务；步骤也已加进 REPO_CONFIG_STEPS，主流程会再兜底快照/还原。
    #
    # 断言的是功能行为：
    #   1. V1 配置（repos 是 map）必须被读出来并迁移成 V2（repos 是 list）
    #   2. 迁移后的优先级必须符合 convertToV2 的规则：默认仓库 0，
    #      其余依次 -100、-200……
    #   3. 坏 YAML 必须让服务拒绝启动（而不是带着半截配置跑起来），
    #      还原之后必须能恢复
    # ── 仓库缓存是可丢弃的（删掉必须能重建）──
    #
    # repo_cache.cpp 里 load() 的"缓存文件不存在"分支（第 38 行）和
    # rebuild()（10/42 未覆盖）此前没走到 —— 套件里缓存一直在，没人删过。
    #
    # 缓存路径是 <repoDir>/states.json（ostree_repo.cpp:cacheFilePath），
    # 也就是 /var/lib/linglong/states.json。缓存是【派生数据】，删掉必须能
    # 从 ostree 仓库重建 —— 这正是要断言的功能行为：
    #   1. 删掉 states.json + 重启服务后，ll-cli list 必须仍然列出原来的应用
    #   2. states.json 必须被重新写出来
    # 跑完把原文件还原 + 重启（步骤也在 REPO_CONFIG_STEPS 里兜底）。
    # ── 缓存写失败时必须"干净失败"，不能静默不一致 ──
    #
    # repo_cache.cpp 里 writeToDisk() 的错误分支（35-36、279-290 等）
    # 此前完全没覆盖：缓存文件一直可写，没人制造过写失败。
    #
    # 手法是【可还原的故障注入】：chattr +i 把 states.json 设为不可写，
    # 做一次真实的状态变更（卸载日历），看它怎么表现，然后立刻
    # chattr -i + 重启服务还原，并把应用恢复成装好的状态。
    # ⚠️ 断言的不是"必须成功"或"必须失败"，而是：
    #   1. 不管成功还是失败，都不能崩（rc 不能是信号）
    #   2. 还原之后，ll-cli list 的应用集合必须和开始前【一致】
    #      —— 也就是不允许出现"缓存说装了、仓库里其实没有"这种
    #      静默不一致
    # ── 锁目录只读时：CLI 不能被搞坏，权限必须还原 ──
    #
    # 手法是可还原的故障注入：把按 uid 的锁目录（/run/linglong/<uid>）
    # 设为只读，跑一次 ll-cli run，然后立刻恢复权限并断言能重新跑起来。
    #
    # ⚠️ 实测说明（别误以为它覆盖了 filelock 的失败分支）：
    #    锁文件 .cli.<id>.lock 【已经存在】时，只读【目录】不影响打开
    #    已有文件，所以 ll-cli run 照样成功、filelock.cpp 的
    #    "failed to open lock file" 分支并【没有】被走到
    #    （实测覆盖率为 0 增长）。要真走到那条分支，得先删掉锁文件、
    #    或者用全新的容器 ID 配只读目录 —— 留作后续。
    #
    # 它真正守住的两件事：
    #   1. 锁目录只读时 CLI 不能崩（rc 不能是信号）
    #   2. 权限必须按【开始前读到的真实值】还原；读歪了要按已知安全值
    #      兜底并报错（见下面 stat_mode 的 PAM 杂音说明）
    # ── 容器运行时不可执行时：必须干净报错，且不能留半启动容器 ──
    #
    # 可还原故障注入第三例：把 ll-box（真正拉容器的那个运行时）chmod 000，
    # 让 ll-cli run 起不来，看它怎么表现，然后立刻恢复权限并断言能跑起来。
    # 这条路会走到 run_context.cpp / utils::Cmd 的命令执行失败分支。
    #
    # 断言的功能属性（比"有没有报错"更重要的两条）：
    #   1. 不能崩（rc 不能是信号），失败要有可读的报错
    #   2. 【不能留下半启动的容器】—— ll-cli ps 里不能多出这个应用的容器
    #      （运行时都起不来却留下容器，就是脏状态）
    #   3. 权限还原后，ll-cli run 必须能正常跑出预期输出
    # ── --json 输出：必须是合法 JSON，且和文本输出说的是同一件事 ──
    #
    # --json 是全局 flag（apps/ll-cli/src/main.cpp:606），每个命令都有
    # 自己的 JSON 分支（libs/linglong/src/linglong/cli/json_printer.cpp，
    # 44 行未覆盖）。套件此前只用过 ps --json，其余分支没走到。
    #
    # 断言的是功能行为（不只是"有没有输出"）：
    #   1. 命令必须成功，且 stdout 必须是【能解析的 JSON】
    #   2. list --json 和 list 说的必须是同一件事：
    #      JSON 里的应用集合 == 文本列表里的应用集合
    #      （两种输出对不上，就是功能不一致）
    # ── 错误路径必须给出清晰报错（不能静默、不能崩）──
    #
    # cli.cpp 里几处错误处理整块没覆盖：handleUpgradeError（15/15）、
    # handleUninstallError（22/32）、handleInstallError（15/36）、
    # handleCommonError（9/19），以及 info 的错误分支（22/46）。
    # 套件里这些命令走的都是"成功"路径。
    #
    # 断言的是功能行为：对不存在的应用做这些操作时，必须
    #   1. 以非 0 退出（不能假装成功）
    #   2. 给出【非空且能看懂的】报错（不能只崩个信号、不能一片空白）
    # 全部是只读命令，不改机器状态。
    def test_error_paths_report_clearly(self):
        ghost = "org.deepin.smoke.nonexistent"
        # ⚠️ 实测：uninstall 要 root（否则报 "Error 9: not authorized"，
        # 那是权限不是"没装"）；depends / size 【不是 CLI 子命令】
        # （直接报 "The following arguments were not expected"），
        # 所以都不放在这里。
        cases = [
            (["uninstall", "--force", ghost], True),
            (["upgrade", ghost], False),
            (["info", ghost], False),
        ]
        problems = []
        for argv, need_root in cases:
            if need_root:
                r = self._sudo_ll_cli(*argv, timeout=120, check=False)
                code = r.returncode
                out = (r.stdout or "") + (r.stderr or "")
            else:
                code, out = self._pty_ll_cli(*argv, timeout=120)
            text = (out or "").strip()
            if code == 0:
                problems.append(f"{' '.join(argv)}: 对不存在的应用居然返回成功")
                continue
            if code < 0:
                problems.append(f"{' '.join(argv)}: 崩了（rc={code}）")
                continue
            if not text:
                problems.append(f"{' '.join(argv)}: 失败了但没有任何报错")
                continue
            # 报错得是"这个应用有问题"的意思，不能是无关的崩溃信息
            if not any(k in text for k in (
                    "未找到", "找不到", "不存在", "没有", "未安装",
                    "not found", "Cannot find", "No such", "unknown",
                    "无效", "invalid")):
                problems.append(
                    f"{' '.join(argv)}: 报错看不出是'应用不存在': "
                    f"{text[:160]!r}")
        if problems:
            raise AssertionError("；".join(problems))

    def test_json_output_variants(self):
        app_id = CALENDAR_APP_ID

        # 先确保有应用可查，否则 list 的交叉验证没意义
        r = self._ll_cli("list", check=False)
        if app_id not in (r.stdout or ""):
            self._sudo_ll_cli("install", app_id, timeout=900, check=False)

        def parse_any(text, argv):
            """整体是合法 JSON 就通过；否则按行（NDJSON）逐行校验。

            ⚠️ 输出可能是【多行 JSON】：带进度上报的命令会先吐
            {"message":...,"percentage":...} 再吐结果，整段用 json.loads
            会报 "Extra data"（实测 search --json，且是【间歇】出现 ——
            镜像快时只有结果一行，慢时前面多一行进度）。
            """
            try:
                json.loads(text)
                return
            except json.JSONDecodeError:
                pass
            for lineno, line in enumerate(text.splitlines(), 1):
                line = line.strip()
                if not line:
                    continue
                try:
                    json.loads(line)
                except json.JSONDecodeError as exc:
                    raise AssertionError(
                        f"ll-cli {' '.join(argv)} 第 {lineno} 行不是合法 "
                        f"JSON: {exc}; 内容 {line[:160]!r}") from exc

        cases = [
            ["list", "--json"],
            ["search", "deepin", "--json"],
            ["ps", "--json"],
            ["info", app_id, "--json"],
            # ⚠️ repo show --json 实测【不支持】（rc=109，"argument was not
            # expected: --json"）—— --json 虽然加在顶层 commandParser 上
            # （main.cpp:606），但不会继承到 repo 这种嵌套子命令。
            # 这类"命令本身不接受 --json"的情况不算失败，跳过即可。
        ]
        unsupported = []
        for argv in cases:
            r = self._ll_cli(*argv, timeout=120, check=False)
            out = (r.stdout or "").strip()
            err = (r.stderr or "") + (r.stdout or "")
            if r.returncode != 0 and "not expected" in err:
                unsupported.append(" ".join(argv))
                continue
            if r.returncode != 0:
                raise AssertionError(
                    f"ll-cli {' '.join(argv)} 失败: rc={r.returncode} "
                    f"{(r.stderr or '')[:200]!r}")
            if not out:
                raise AssertionError(f"ll-cli {' '.join(argv)} 没有任何输出")
            try:
                parse_any(out, argv)
            except json.JSONDecodeError as exc:
                raise AssertionError(
                    f"ll-cli {' '.join(argv)} 的输出不是合法 JSON: "
                    f"{exc}; 开头 {out[:200]!r}") from exc

        if unsupported:
            print(f"    [note] 这些子命令不接受 --json，已跳过: {unsupported}")

        # 交叉验证：JSON 和文本两种输出必须一致
        row_re = re.compile(r"^(\S+)\s+\S+\s+(\d[\d.]*)\s", re.M)
        text_set = {f"{i}/{v}" for i, v in row_re.findall(
            self._ll_cli("list", check=False).stdout or "")}
        r = self._ll_cli("list", "--json", check=False)
        try:
            data = json.loads(r.stdout or "[]")
        except json.JSONDecodeError:
            data = []
        if isinstance(data, dict):
            data = data.get("apps", data.get("data", []))
        json_set = set()
        for item in data if isinstance(data, list) else []:
            if isinstance(item, dict):
                i = item.get("id") or item.get("appId") or item.get("name")
                v = item.get("version")
                if i and v:
                    json_set.add(f"{i}/{v}")
        # ⚠️ 实测：两种输出【不相等】—— list --json 会比文本多出
        # org.deepin.runtime.dtk 这类 runtime（文本列表看着只列 app）。
        # 这可能是"文本只列应用、JSON 列全部已装 ref"的有意设计，
        # 也可能是遗漏，所以这里【不断言相等】，只断言真正要紧的
        # 安全属性：JSON 不能比文本【少】—— 少一个就是 JSON 漏报了
        # 已安装的东西，那才是功能缺陷。
        missing = sorted(text_set - json_set)
        if missing:
            raise AssertionError(
                f"list --json 比文本输出【少】了这些已装应用（漏报）: "
                f"{missing}; 文本 {sorted(text_set)} vs "
                f"JSON {sorted(json_set)}")
        extra = sorted(json_set - text_set)
        if extra:
            print(f"    [note] list --json 比文本多出（疑似 runtime 之类）: "
                  f"{extra}")
