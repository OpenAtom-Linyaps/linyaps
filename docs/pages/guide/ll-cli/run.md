<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# 运行应用

`ll-cli run`命令可以启动一个如意玲珑应用。

查看 `ll-cli run`命令的帮助信息：

```bash
ll-cli run --help
```

`ll-cli run`命令的帮助信息如下：

```text
运行应用程序
用法: ll-cli run [选项] 应用程序 [命令...]

示例:
# 通过应用名运行应用程序
ll-cli run org.deepin.demo
# 在容器中执行命令而不是运行应用程序
ll-cli run org.deepin.demo bash
ll-cli run org.deepin.demo -- bash
ll-cli run org.deepin.demo -- bash -x /path/to/bash/script

Positionals:
  APP TEXT REQUIRED           指定应用程序名
  COMMAND TEXT ...            在正在运行的沙盒中运行命令

Options:
  -h,--help                   打印帮助信息并退出
  --help-all                  展开所有帮助
  --file FILES:FILE ...       将文件传递到沙盒中运行的应用程序
  --url URLS ...              将URL传递到沙盒中运行的应用程序

如果在使用过程中遇到任何问题，
您可以通过此项目向如意玲珑项目团队报告错误：https://github.com/OpenAtom-Linyaps/linyaps/issues
```

当应用被正常安装后，使用 `ll-cli run`命令即可启动：

```bash
ll-cli run org.deepin.calculator
```

使用 `ll-cli run`命令可以进入指定程序容器环境：

```bash
ll-cli run org.deepin.calculator --exec /bin/bash
```

进入后可执行 `shell` 命令，如 `gdb`、`strace`、`ls`、`find`等。

由于如意玲珑应用都是在容器内运行，无法通过常规的方式直接调试，需要在容器内运行调试工具，如 `gdb`：

```bash
gdb /opt/apps/org.deepin.calculator/files/bin/deepin-calculator
```

该路径为容器内应用程序的绝对路径。

如意玲珑应用 `release`版本更多调试信息请参考：[常见运行问题](../debug/faq.md)。
