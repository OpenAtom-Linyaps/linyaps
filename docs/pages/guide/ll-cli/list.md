<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# 列出已安装的应用

`ll-cli list`命令可以查看已安装的玲珑应用。

查看 `ll-cli list`命令的帮助信息：

```bash
ll-cli list --help
```

`ll-cli list`命令的帮助信息如下：

```text
Usage: ll-cli [options] list

Options:
  -h, --help                           Displays help on commandline options.
  --help-all                           Displays help including Qt specific
                                       options.
  --type <--type=installed>            query installed app
  --repo-point                         app repo type to use
  --nodbus                             execute cmd directly, not via dbus

Arguments:
  list                                 show installed application
```

查看已安装的`runtime` 和应用，运行 `ll-cli list`命令：

```bash
ll-cli list
```

`ll-cli list` 输出如下：

```text
appId                           name                            version         arch        channel         module      description
org.deepin.Runtime              runtime                         20.5.0          x86_64      linglong        runtime     runtime of deepin
org.deepin.calculator           deepin-calculator               5.7.21.4        x86_64      linglong        runtime     calculator for deepin os
org.deepin.camera               deepin-camera                   6.0.2.6         x86_64      linglong        runtime     camera for deepin os
```
