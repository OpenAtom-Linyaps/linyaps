<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# ll-cli简介

`ll-cli`是一个包管理器前端，用于管理玲珑应用的安装、卸载、查看、启动、关闭、调试、更新等操作。

查看`ll-cli`命令的帮助信息：

```bash
ll-cli --help
```

`ll-cli`命令的帮助信息如下：

```text
Usage: ll-cli [options] subcommand [sub-option]

Options:
  -h, --help  Displays help on commandline options.
  --help-all  Displays help including Qt specific options.

Arguments:
  subcommand  run
              ps
              exec
              kill
              install
              uninstall
              update
              query
              list
```
