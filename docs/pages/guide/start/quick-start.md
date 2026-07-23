<!--
SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# 快速上手

本章面向使用如意玲珑应用的终端用户。完成下面的步骤后，您将能够使用 `ll-cli` 搜索、安装、运行、升级和卸载应用。

## 准备 ll-cli

deepin/UOS 的部分版本已预装 `ll-cli`。先在终端确认当前版本：

```bash
ll-cli --version
```

如果提示找不到命令，请按照[安装如意玲珑](./install.md)中与当前 Linux 发行版对应的步骤安装 `linglong-bin`。追求稳定的用户推荐使用 release 仓库，以获取当前稳定版本；需要提前体验尚未发布的功能时，再按页面说明使用 latest 仓库。

## 日常使用

### 搜索应用

```bash
ll-cli search calculator
```

搜索结果中的 `ID` 是后续命令使用的应用标识。更多筛选方式见 [`ll-cli search`](../reference/commands/ll-cli/search.md)。

### 安装应用

```bash
ll-cli install org.deepin.calculator
```

`ll-cli` 也可以安装本地 UAB 或 layer 文件。完整参数见 [`ll-cli install`](../reference/commands/ll-cli/install.md)。

### 查看已安装的应用

忘记应用 ID、需要确认应用是否已安装，或者想查看已安装版本时，使用：

```bash
ll-cli list
```

只查看可以升级的应用：

```bash
ll-cli list --upgradable
```

还可以按 App、Runtime 或 Base 类型筛选结果，完整参数见 [`ll-cli list`](../reference/commands/ll-cli/list.md)。

### 运行应用

```bash
ll-cli run org.deepin.calculator
```

需要向应用传递环境变量使用 `--env` 参数，需要控制其他特定行为请参阅 [`ll-cli run`](../reference/commands/ll-cli/run.md)。

### 查看正在运行的应用

需要确认应用是否仍在运行，或者排查重复启动、无窗口但进程仍存在等问题时，使用：

```bash
ll-cli ps
```

输出中会显示应用 ID、容器 ID 和进程 ID。完整参数见 [`ll-cli ps`](../reference/commands/ll-cli/ps.md)。

### 停止运行中的应用

应用无响应、无法正常退出，或者需要在升级前停止旧进程时，先通过 `ll-cli ps` 确认应用 ID，再执行：

```bash
ll-cli kill org.deepin.calculator
```

该命令默认发送 `SIGTERM`。如果应用仍然无法退出，可以发送信号 9（`SIGKILL`）强制终止：

```bash
ll-cli kill -s 9 org.deepin.calculator
```

强制终止不会给应用保存数据或清理资源的机会，可能造成未保存的数据丢失，因此只应在普通 `kill` 无效时使用。完整参数见 [`ll-cli kill`](../reference/commands/ll-cli/kill.md)。

### 升级应用

升级全部已安装应用：

```bash
ll-cli upgrade
```

只升级指定应用：

```bash
ll-cli upgrade org.deepin.calculator
```

完整参数见 [`ll-cli upgrade`](../reference/commands/ll-cli/upgrade.md)。

### 卸载应用

```bash
ll-cli uninstall org.deepin.calculator
```

完整参数见 [`ll-cli uninstall`](../reference/commands/ll-cli/uninstall.md)。

## 查看命令帮助

不确定有哪些子命令或参数时，可以使用 `--help` 查看帮助。查看 `ll-cli` 的全局选项和子命令列表：

```bash
ll-cli --help
```

将 `--help` 放在子命令后，可以查看该子命令的参数和用法。例如，查看 `install` 命令的帮助：

```bash
ll-cli install --help
```

其他子命令使用相同方式，例如：

```bash
ll-cli run --help
ll-cli list --help
ll-cli kill --help
```

如果普通帮助隐藏了部分高级选项，可以使用 `--help-all` 展开完整帮助：

```bash
ll-cli --help-all
ll-cli run --help-all
```


## 继续阅读

- [`ll-cli` 命令参考](../reference/commands/ll-cli/ll-cli.md)列出了所有子命令。
- [常见问题](../tips-and-faq/faq.md)汇总运行、数据目录、桌面集成和依赖相关问题。
- 排查问题时，可在命令后增加 `--verbose` 获取详细日志，并将完整命令、日志、系统版本和 `ll-cli --version` 的结果一并提供给社区。
- 如果您准备构建应用，请继续阅读[构建第一个如意玲珑应用](./build_your_first_app.md)。
