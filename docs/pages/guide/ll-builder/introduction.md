<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# ll-builder简介

`ll-builder`是为应用开发者提供的一款构建玲珑应用工具。

主要功能如下：

- 支持在独立容器内构建。
<!-- - 定义了一套版本管理系统。 -->
- 提供 sdk 开发套件列表。
<!-- - 包含完整推送发布流程。 -->

查看`ll-builder`命令的帮助信息：

```bash
ll-builder --help
```

ll-builder 命令的帮助信息如下：

```text
Usage: ll-builder [options] subcommand [sub-option]

Options:
  -v, --verbose  show detail log
  -h, --help     Displays help on commandline options.
  --help-all     Displays help including Qt specific options.

Arguments:
  subcommand     create
                 build
                 run
                 export
                 push
                 convert
```
