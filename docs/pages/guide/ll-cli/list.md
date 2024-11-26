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
列出已安装的应用程序或运行时
用法: ll-cli list [选项]

示例:
# 显示已安装的应用程序
ll-cli list
# 显示已安装的运行时
ll-cli list --type=runtime
# 显示当前已安装应用程序的最新版本列表
ll-cli list --upgradable


Options:
  -h,--help                   打印帮助信息并退出
  --help-all                  展开所有帮助
  --type TYPE [app]           使用指定类型过滤结果。“runtime”、“app”或“all”之一
  --upgradable                显示当前已安装应用程序的最新版本列表，仅适用于应用程序

如果在使用过程中遇到任何问题，
您可以通过此项目向如意玲珑项目团队报告错误：https://github.com/OpenAtom-Linyaps/linyaps/issues
```

查看已安装的应用，运行 `ll-cli list`命令：

```bash
ll-cli list
```

`ll-cli list` 输出如下：

```text
id                               name                             version         arch        channel         module      description
org.dde.calendar                 dde-calendar                     5.14.5.0        x86_64      main            binary      calendar for deepin os.
org.deepin.browser               deepin-browser                   6.5.5.4         x86_64      main            binary      browser for deepin os.
```

查看已安装的运行环境，运行 `ll-cli list --type=runtime`命令：

```bash
ll-cli list --type=runtime
```

Here is the output:

```text
id                               name                             version         arch        channel         module      description
org.deepin.Runtime               deepin runtime                   20.0.0.9        x86_64      main            runtime     Qt is a cross-platform C++ application framework. Qt'...
org.deepin.base                  deepin-foundation                23.1.0.2        x86_64      main            binary      deepin base environment.
org.deepin.Runtime               deepin runtime                   23.0.1.2        x86_64      main            runtime     Qt is a cross-platform C++ application framework. Qt'...
org.deepin.foundation            deepin-foundation                23.0.0.27       x86_64      main            runtime     deepin base environment.
```

显示当前已安装应用程序的最新版本列表，仅适用于应用程序。运行 `ll-cli list --upgradable`:

`ll-cli list --upgradable`输出如下:

```text
id                               installed       new
com.qq.wemeet.linyaps            3.19.2.402      3.19.2.403
org.dde.calendar                 5.14.5.0        5.14.5.1
```
