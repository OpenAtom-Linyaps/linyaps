<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# 从远程仓库查询应用

`ll-cli search`命令可以查询玲珑远程仓库中的应用信息。

查看 `ll-cli search`命令的帮助信息：

```bash
ll-cli search --help
```

`ll-cli search`命令的帮助信息如下：

```text
Usage: ll-cli [options] search com.deepin.demo

Options:
  -h, --help                           Displays help on commandline options.
  --help-all                           Displays help including Qt specific
                                       options.
  --repo-point                         app repo type to use

Arguments:
  search                               search app info
  appId                                application id
```

通过 `ll-cli search`命令可以从远程 repo 中查找应用程序信息:

```bash
ll-cli search calculator
```

该命令将返回 `appid`(appid 是应用唯一标识) 中包含 calculator 关键词的所有应用程序信息，包含完整的 `appid`、应用程序名称、版本、平台及应用描述信息。

`ll-cli search calculator`输出如下：

```text
appId                           name                            version         arch        channel         module      description
org.deepin.calculator           deepin-calculator               5.5.23          x86_64      linglong        runtime     Calculator for UOS
org.deepin.calculator           deepin-calculator               5.7.1           x86_64      linglong        runtime     Calculator for UOS

```
