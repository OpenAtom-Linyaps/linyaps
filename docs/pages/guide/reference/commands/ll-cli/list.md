% ll-cli-list 1

## NAME

ll\-cli\-list - 列出已安装的应用程序或运行时

## SYNOPSIS

**ll-cli list** [*options*]

## DESCRIPTION

`ll-cli list` 命令可以查看已安装的玲珑应用。该命令用于显示系统中已安装的应用程序、运行时和基础环境的详细信息。

## OPTIONS

**-h, --help**
: 打印帮助信息并退出

**--help-all**
: 展开所有帮助

**--type** _TYPE_ [*all*]
: 使用指定类型过滤结果。"runtime"、"base"、"app"或 "all"之一

**--upgradable**
: 显示当前已安装应用程序的最新版本列表，仅适用于应用程序

## EXAMPLES

查看已安装的应用，运行 `ll-cli list` 命令：

```bash
ll-cli list
```

`ll-cli list` 输出如下：

```text
id                               name                             version         arch        channel         module      description
org.dde.calendar                 dde-calendar                     5.14.5.0        x86_64      main            binary      calendar for deepin os.
org.deepin.browser               deepin-browser                   6.5.5.4         x86_64      main            binary      browser for deepin os.
```

查看已安装的运行环境，运行 `ll-cli list --type=runtime` 命令：

```bash
ll-cli list --type=runtime
```

输出如下：

```text
id                               name                             version         arch        channel         module      description
org.deepin.Runtime               deepin runtime                   20.0.0.9        x86_64      main            runtime     Qt is a cross-platform C++ application framework. Qt'...
org.deepin.base                  deepin-foundation                23.1.0.2        x86_64      main            binary      deepin base environment.
org.deepin.Runtime               deepin runtime                   23.0.1.2        x86_64      main            runtime     Qt is a cross-platform C++ application framework. Qt'...
org.deepin.foundation            deepin-foundation                23.0.0.27       x86_64      main            runtime     deepin base environment.
```

显示当前已安装应用程序的最新版本列表，仅适用于应用程序。运行 `ll-cli list --upgradable`:

`ll-cli list --upgradable` 输出如下:

```text
id                               installed       new
com.qq.wemeet.linyaps            3.19.2.402      3.19.2.403
org.dde.calendar                 5.14.5.0        5.14.5.1
```

## SEE ALSO

**[ll-cli(1)](./ll-cli.md)**, **[ll-cli-info(1)](info.md)**, **[ll-cli-search(1)](search.md)**

## HISTORY

2023年，由 UnionTech Software Technology Co., Ltd. 开发
