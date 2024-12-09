<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# 强制退出应用

`ll-cli kill`命令可以强制退出正在运行的玲珑应用。

查看`ll-cli kill`命令的帮助信息：

```bash
ll-cli kill --help
```

`ll-cli kill` 命令的帮助信息如下：

```text
停止运行的应用程序
用法: ll-cli kill [选项] 应用

Positionals:
  APP TEXT REQUIRED           指定正在运行的应用程序名

Options:
  -h,--help                   打印帮助信息并退出
  --help-all                  展开所有帮助

如果在使用过程中遇到任何问题，
您可以通过此项目向如意玲珑项目团队报告错误：https://github.com/OpenAtom-Linyaps/linyaps/issues
```

使用 `ll-cli kill` 命令可以强制退出正在运行的玲珑应用:

```bash
ll-cli kill org.deepin.calculator
```
