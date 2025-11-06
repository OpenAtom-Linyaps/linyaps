% ll-cli-info 1

## NAME

ll\-cli\-info - 显示已安装的应用程序或运行时的信息

## SYNOPSIS

**ll-cli info** [*options*] _app_

## DESCRIPTION

使用 `ll-cli info` 显示已安装的应用程序或运行时的信息。该命令以 JSON 格式显示应用程序或运行时的详细信息，包括架构、基础环境、运行时、描述等。

## OPTIONS

**-h, --help**
: 打印帮助信息并退出

**--help-all**
: 展开所有帮助

## POSITIONAL ARGUMENTS

**APP** _TEXT_ _REQUIRED_
: 指定应用程序名，也可以是如意玲珑.layer文件

## EXAMPLES

使用 `ll-cli info` 显示已安装的应用程序或运行时的信息：

```bash
ll-cli info org.dde.calendar
```

`ll-cli info org.dde.calendar` 输出如下:

```text
{
    "arch": [
        "x86_64"
    ],
    "base": "main:org.deepin.base/23.1.0/x86_64",
    "channel": "main",
    "command": [
        "dde-calendar"
    ],
    "description": "calendar for deepin os.\n",
    "id": "org.dde.calendar",
    "kind": "app",
    "module": "binary",
    "name": "dde-calendar",
    "runtime": "main:org.deepin.runtime.dtk/23.1.0/x86_64",
    "schema_version": "1.0",
    "size": 13483249,
    "version": "5.14.5.1"
}
```

## SEE ALSO

**[ll-cli(1)](./ll-cli.md)**, **[ll-cli-list(1)](list.md)**, **[ll-cli-content(1)](content.md)**

## HISTORY

2023年，由 UnionTech Software Technology Co., Ltd. 开发
