<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# 安装应用

`ll-cli install`命令用来安装玲珑应用。

查看 `ll-cli install`命令的帮助信息：

```bash
ll-cli install --help
```

`ll-cli install`命令的帮助信息如下：

```text
Usage: ll-cli [options] install com.deepin.demo

Options:
  -h, --help                           Displays help on commandline options.
  --help-all                           Displays help including Qt specific
                                       options.
  --repo-point app                     repo type to use
  --nodbus                             execute cmd directly, not via dbus(only
                                       for root user)
  --channel <--channel=linglong>       the channel of app
  --module <--module=runtime>          the module of app

Arguments:
  install                              install an application
  appId                                application id
```

运行 `ll-cli install`命令安装玲珑应用:

```bash
ll-cli install <org.deepin.calculator>
```

`ll-cli install`命令需要输入应用完整的 `appid`，若仓库有多个版本则会默认安装最高版本。

安装指定版本需在 `appid`后附加对应版本号:

```bash
ll-cli install <org.deepin.calculator/5.1.2>
```

`ll-cli install org.deepin.calculator`输出如下：

```text
install org.deepin.calculator , please wait a few minutes...
org.deepin.calculator is installing...
message: install org.deepin.calculator, version:5.7.21.4 success
```

应用安装完成后，客户端会显示安装结果信息。

我们在使用 `ll-builder export` 命令导出的 layer 文件，可以使用 `ll-cli install` 进行安装。

```bash
ll-cli install ./com.baidu.baidunetdisk_4.17.7.0_x86_64_runtime.layer
```

可以使用 `ll-cli list | grep com.baidu.baidunetdisk` 命令来查看是否安装成功。

使用下面的命令运行应用。

```bash
ll-cli run com.baidu.baidunetdisk
```
