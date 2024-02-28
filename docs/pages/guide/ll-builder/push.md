<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# 推送uab到远程仓库

`ll-builder push`命令用来将玲珑软件包推送至玲珑远程仓库。

查看`ll-builder push`命令的帮助信息：

```bash
ll-builder push --help
```

`ll-builder push`命令的帮助信息如下：

```text
Usage: ll-builder [options] push <filePath>

Options:
  -v, --verbose  show detail log
  -h, --help     Displays this help.
  --force        force push true or false

Arguments:
  push           push build result to repo
  filePath       bundle file path
```

`ll-builder push`命令根据输入的文件路径，读取`bundle`格式软件包内容，将软件数据及`bundle`格式软件包传输到服务端。

```bash
ll-builder push <org.deepin.demo-1.0.0_x86_64.uab>
```
