<!--
SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# 快速上手

本章面向使用如意玲珑应用的终端用户。完成下面的步骤后，您将能够使用 `ll-cli` 搜索、安装和运行应用。

## 准备 ll-cli

deepin/UOS 的部分版本已预装 `ll-cli`。先在终端确认当前版本：

```bash
ll-cli --version
```

如果提示找不到命令，可以使用下面的一行命令快速安装：

```bash
curl -fsSL https://get.linyaps.org.cn | sh
```

安装脚本会识别当前 Linux 发行版，并在需要时请求 `sudo` 权限。安装完成后，就可以开始体验如意玲珑应用了。

如果您是应用开发者，使用下面命令快速安装，会同时安装 `ll-builder` 构建工具：

```bash
curl -fsSL https://get.linyaps.org.cn | sh -s -- full
```

如果您的发行版不受支持或者需要手动配置仓库，请参阅[安装如意玲珑](./install.md)。

## 安装并运行应用

以计算器应用为例，先搜索名称中包含 `calculator` 的应用：

```bash
ll-cli search calculator
```

在搜索结果中可以看到类似下面的内容：

```
ID                                         名称                             版本            渠道            模块        仓库      描述
...
org.deepin.calculator                      deepin-calculator                6.5.37.1        main            binary      stable    Calculator for UOS
```

计算器应用的 ID 为 `org.deepin.calculator`，接下来安装这个 ID 的应用：

```bash
ll-cli install org.deepin.calculator
```

等待安装完成，再运行计算器：

```bash
ll-cli run org.deepin.calculator
```

看到计算器窗口后，就说明您已经完成了第一次如意玲珑安装、运行应用的体验。

## 继续阅读

* 需要查看、停止、升级或卸载应用时，请继续阅读[通过 CLI 管理应用](./manage-apps-with-cli.md)。
* 如果您准备构建应用，请继续阅读[构建第一个如意玲珑应用](./build_your_first_app.md)。
* [ll-cli](../reference/commands/ll-cli/ll-cli.md) 命令参考列出了所有子命令。
* [常见问题](../tips-and-faq/faq.md)汇总运行、数据目录、桌面集成和依赖相关问题。
