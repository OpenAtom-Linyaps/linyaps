<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# 查看运行中的应用

`ll-cli ps`命令可以查看正在运行的玲珑应用。

查看`ll-cli ps`命令的帮助信息：

```bash
ll-cli ps --help
```

`ll-cli ps` 命令的帮助信息如下：

```text
Usage: ll-cli [options] ps

Options:
  -h, --help                 Displays help on commandline options.
  --help-all                 Displays help including Qt specific options.
  --output-format <console>  json/console

Arguments:
  ps                         show running applications
```

查看正在运行的应用，运行`ll-cli ps`命令：

```bash
ll-cli ps
```

`ll-cli ps`命令输出如下：

```text
App                                             ContainerID                         Pid     Path
org.deepin.calculator/5.7.21.4/x86_64           7c4299db7f5647428a79896658efa35c    1943975 /run/user/1000/linglong/7c4299db7f5647428a79896658efa35c
```
