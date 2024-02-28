<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# 卸载应用

`ll-cli uninstall`命令可以卸载玲珑应用。

查看`ll-cli uninstall`命令的帮助信息：

```bash
ll-cli uninstall --help
```

`ll-cli uninstall`命令的帮助信息如下：

```text
Usage: ll-cli [options] uninstall com.deepin.demo

Options:
  -h, --help                           Displays this help.
  --default-config                     default config json filepath
  --nodbus                             execute cmd directly, not via dbus
  --repo-point                         app repo type to use

Arguments:
  uninstall                            uninstall an application
  appId                                application id
```

使用`ll-cli uninstall`命令卸载玲珑应用：

```bash
ll-cli uninstall <org.deepin.calculator>
```

`ll-cli uninstall org.deepin.calculator`命令输出如下：

```text
uninstall org.deepin.calculator , please wait a few minutes...
message: uninstall org.deepin.calculator, version:5.7.21.4 success
```

默认卸载最高版本，可以通过`appid`后附加对应版本号卸载指定版本：

```bash
ll-cli uninstall <org.deepin.calculator/5.1.2>
```

该命令执行成功后，该玲珑应用将从系统中被卸载掉。
