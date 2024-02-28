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
Usage: ll-cli [options] kill container-id

Options:
  -h, --help        Displays this help.
  --default-config  default config json filepath

Arguments:
  kill              kill container with id
  container-id      container id
```

使用 `ll-cli kill` 命令可以强制退出正在运行的玲珑应用:

```bash
ll-cli kill <9c41c0af2bad4617aea8485f5aaeb93a>
```

`ll-cli kill 9c41c0af2bad4617aea8485f5aaeb93a`命令输出如下：

```text
kill app:org.deepin.calculator/5.7.21.4/x86_64 success
```
