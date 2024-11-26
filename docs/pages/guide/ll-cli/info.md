<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# 显示应用信息

使用`ll-cli info`显示已安装的应用程序或运行时的信息。

查看`ll-cli info`命令的帮助信息：

```bash
ll-cli info --help
```

`ll-cli info`命令的帮助信息如下：

```text
显示已安装的应用程序或运行时的信息
用法: ll-cli info [选项] 应用

Positionals:
  APP TEXT REQUIRED           指定应用程序名，也可以是如意玲珑.layer文件

Options:
  -h,--help                   打印帮助信息并退出
  --help-all                  展开所有帮助

如果在使用过程中遇到任何问题，
您可以通过此项目向如意玲珑项目团队报告错误：https://github.com/OpenAtom-Linyaps/linyaps/issues
```

使用 `ll-cli info`显示已安装的应用程序或运行时的信息。

`ll-cli info org.dde.calendar`输出如下:

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
