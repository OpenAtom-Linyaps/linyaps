<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# 运行编译后的应用

`ll-builder run`命令可以运行编译后的可执行程序。

查看`ll-builder run`命令的帮助信息：

```bash
ll-builder run --help
```

`ll-builder run`命令的帮助信息如下：

```text
Usage: ll-builder [options] run

Options:
  -v, --verbose  show detail log
  -h, --help     Displays this help.
  --exec <exec>  run exec than build script

Arguments:
  run            run project
```

`ll-builder run`命令根据配置文件读取该程序相关的运行环境信息，构造一个容器环境，并在容器中执行该程序而无需安装。

```bash
ll-builder run
```

`ll-builder run`运行成功输出如下：

![org.deepin.demo.png](./images/org.deepin.demo.png)

为了便于调试，使用额外的`--exec /bin/bash`参数可替换进入容器后默认执行的程序，如：

```bash
ll-builder run --exec /bin/bash
```

使用该选项，`ll-builder`创建容器后将进入`bash`终端，可在容器内执行其他操作。
