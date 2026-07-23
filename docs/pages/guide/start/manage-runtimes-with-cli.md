<!--
SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# 通过 CLI 管理运行时

本章介绍如何使用 `ll-cli` 管理已安装的运行时（关于运行时应先了解 Base、Runtime 和 App 的分层关系，参见[基础概念介绍](../reference/basic-concepts.md)）。主要面向需要排查依赖、控制磁盘占用或手动控制运行环境的高级用户。运行时通常由应用安装过程自动拉取并被多个应用共享，日常使用不需要单独操作运行时，只有在诊断兼容性问题、回收磁盘空间或修复异常依赖状态时，才需要使用本章中的高级操作。

## 查看已安装的运行环境

分别查看已安装的 Runtime 和 Base：

```bash
ll-cli list --type=runtime
ll-cli list --type=base
```

系统中可能同时存在同一 Runtime 的多个版本。后续操作存在歧义时，应使用包含版本的引用，例如：

```text
org.deepin.runtime.dtk/23.1.0
```

完整的引用由渠道、ID、版本和架构等字段组成，详细格式见[引用（Ref）](../extra/ref.md)。

## 清理无效 Runtime

这里的“无效 Runtime”是指当前没有被任何已安装 App 引用的 Runtime；未被引用的 Base 也会同时纳入清理范围。常见来源包括应用卸载后遗留的依赖，以及应用升级后不再使用的旧版本。

执行：

```bash
ll-cli prune
```

`prune` 会根据已安装 App 的依赖关系保留仍在使用的 Base、Runtime 和相关模块，并直接删除不再被引用的内容。该命令没有仅预览而不删除的选项；执行结果会列出已移除的 Base 和 Runtime。

完整参数见 [`ll-cli prune`](../reference/commands/ll-cli/prune.md)。

## 分析应用依赖

查看所有已安装应用的依赖树：

```bash
ll-cli analyze depends
```

只分析一个应用：

```bash
ll-cli analyze depends org.deepin.calculator
```

输出结果类似如下内容：

```
main:org.deepin.base/25.2.2.6/x86_64 (base)
├── main:org.deepin.deepin-wine/11.0.0.0/x86_64 (runtime)
│   └── main:com.qq.weixin.work.deepin/5.0.7.0/x86_64 (app)
├── main:org.deepin.runtime.dtk/25.2.2.2/x86_64 (runtime)
│   ├── main:cn.org.linyaps.testsuite.portal/1.0.0.0/x86_64 (app)
│   ├── main:org.blender.lts/4.5.8.0/x86_64 (app)
│   └── main:org.deepin.camera/6.5.39.1/x86_64 (app)
├── main:org.deepin.runtime.webengine/25.2.2.4/x86_64 (runtime)
│   └── main:org.deepin.editor/6.5.47.1/x86_64 (app)
├── main:com.qq.wemeet/3.26.10.404/x86_64 (app)
├── main:com.tencent.wechat/4.1.1.6/x86_64 (app)
├── main:com.windsurf.editor/1.9600.41.0/x86_64 (app)
└── main:org.deepin.driver.media.intel/25.0.0.0/x86_64 (extension)
```

依赖树以 Base 为根节点，依次显示 Runtime、App 和相关 Extension。多个应用使用同一个 Runtime 时，会出现在同一个 Runtime 节点下。通过该结果可以看出：

- 哪些应用正在使用某个 Runtime。
- 一个应用实际解析到了哪个 Base 和 Runtime 版本。
- 同一 Runtime 是否被多个应用共享。

`analyze depends` 的参数必须是已安装的 App，不能直接传入 Runtime ID。需要反向查找 Runtime 的使用者时，执行不带参数的命令，并在完整依赖树中定位目标 Runtime。

该命令分析的是本机已安装内容形成的运行依赖树，而不是应用声明的或者在远程仓库中的依赖状态。需要机器可读的结果时，在 `ll-cli` 后使用全局 `--json` 选项：

```bash
ll-cli --json analyze depends
```

## 分析磁盘占用

查看所有已安装模块的空间占用：

```bash
ll-cli analyze size
```

如意玲珑使用 OSTree 存储内容，不同 App、Runtime 和 Base 之间可以共享相同数据。因此，不能简单地把每个目录的表观大小相加。`analyze size` 提供以下指标：

| 指标 | 含义 |
| --- | --- |
| Exclusive | 仅当前模块使用的数据块；删除该模块后，这部分空间具备直接释放条件。 |
| Shared | 当前模块与其他模块共享的数据块；只删除当前模块通常不会释放这部分空间。 |
| Logical | 当前模块逻辑上使用的全部空间，即 Exclusive 与 Shared 之和。 |
| Actual | 当前模块的估算实际占用；独占空间全部计入，共享空间按使用该数据的模块数量分摊。 |
| Calculated actual total size | 对当前已安装模块去重后计算的实际占用总量。 |
| Repository real size | 本地仓库目录的真实磁盘占用，还可能包含仓库元数据、缓存及未被当前模块引用的对象。 |

默认按 Actual 从大到小排序。也可以按其他字段排序：

```bash
ll-cli analyze size --sort exclusive
ll-cli analyze size --sort shared
ll-cli analyze size --sort logical
ll-cli analyze size --sort id
```

增加 `--asc` 可以改为升序：

```bash
ll-cli analyze size --sort actual --asc
```

JSON 输出中的大小以字节为单位，适合交给脚本继续分析：

```bash
ll-cli --json analyze size
```

评估清理收益时，应优先关注 Exclusive 和 Repository real size。Shared 或 Logical 很大的 Runtime 不一定能通过单独删除释放同等空间。

## 强制覆盖应用使用的 Runtime

排查 Runtime 兼容性问题时，可以在单次启动中覆盖应用声明的 Runtime：

```bash
ll-cli run org.deepin.calculator \
  --runtime main:org.deepin.runtime.dtk/23.1.0/x86_64
```

指定的 Runtime 必须已经安装并与应用使用的 Base 兼容。该参数只影响本次启动，不会修改应用包的元数据，也不会改变应用下次正常启动时使用的 Runtime。

覆盖 Runtime 可能引入 ABI 不兼容、缺少动态库或插件路径变化等问题。同样也可以使用 `--base` 同时指定 Base，但 Base 与 Runtime 必须配套。

Runtime 支持多版本共存，安装另一个版本不会覆盖已经安装的版本。需要测试其他版本时，先查询并安装目标版本：

```bash
ll-cli search org.deepin.runtime.dtk --type=runtime --show-all-version
ll-cli install org.deepin.runtime.dtk/23.1.0
```

然后通过 `ll-cli run --runtime` 为单次启动选择该版本。不同版本会继续同时保留，直到不再被应用引用并由 `ll-cli prune` 清理，或被明确强制卸载。

## 强制卸载 Runtime

普通卸载会拒绝直接移除 Base 或 Runtime，避免破坏仍在使用的依赖。清理未使用的 Runtime 应始终优先使用 `ll-cli prune`。

只有在 Runtime 元数据损坏、依赖关系无法正常修复，或明确需要构造测试环境时，才使用强制卸载。

推荐使用完整版本引用卸载：

```bash
ll-cli uninstall \
  main:org.deepin.runtime.dtk/23.1.0/x86_64 \
  --force
```

`--force` 会跳过 Base 和 Runtime 的卸载保护。它不会自动卸载依赖该 Runtime 的应用，也不会为这些应用选择替代 Runtime。强制卸载后，相关应用可能无法启动，重新安装原 Runtime，或重新安装受影响的应用以恢复其依赖。

如果同一 Runtime 安装了多个版本而命令没有指定版本，`ll-cli` 会提示候选项以供选择。
