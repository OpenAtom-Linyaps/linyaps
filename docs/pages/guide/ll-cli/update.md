<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# 更新应用

`ll-cli update`命令可以更新玲珑应用。

查看`ll-cli update`命令的帮助信息：

```bash
ll-cli update --help
```

`ll-cli update`命令的帮助信息如下：

```text
Usage: ll-cli [options] update com.deepin.demo

Options:
  -h, --help                      Displays help on commandline options.
  --help-all                      Displays help including Qt specific options.
  --channel <--channel=linglong>  the channel of app
  --module <--module=runtime>     the module of app

Arguments:
  update                          update an application
  appId                           application id
```

通过`ll-cli update`命令将本地软件包版本更新到远端仓库中的最新版本，如:

```bash
ll-cli update <org.deepin.calculator>
```

`ll-cli update org.deepin.calculator`命令输出如下：

```text
update org.deepin.calculator , please wait a few minutes...
org.deepin.calculator is updating...
message: update org.deepin.calculator success, version:5.7.16 --> 5.7.21.4
```

更新指定版本到最新版本:

```bash
ll-cli update <org.deepin.calculator/5.7.16>
```
