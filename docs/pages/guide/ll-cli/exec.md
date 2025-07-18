<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# 进入容器内部

`ll-cli exec`命令可以进入正在运行的容器内部。

查看`ll-cli exec`命令的帮助信息：

```bash
ll-cli exec --help
```

`ll-cli exec`命令的帮助信息如下：

```text
在当前运行的沙盒中执行命令。
用法: ll-cli [选项] [子命令]

Positionals:
  INSTANCE TEXT REQUIRED      指定正在运行的应用程序实例（可以通过 ps 命令获取）
  COMMAND TEXT ...            在正在运行的沙盒中运行命令

Options:
  -h,--help                   打印帮助信息并退出
  --help-all                  展开所有帮助
  --working-directory PATH:DIR
                              指定工作目录

如果在使用过程中遇到任何问题，
您可以通过此项目向如意玲珑项目团队报告错误：https://github.com/OpenAtom-Linyaps/linyaps/issues
```

新开一个终端，执行如下命令：

使用`ll-cli exec`进入正在运行的容器内部：

1. 使用 `ll-cli ps` 获取正在运行的容器ID:

```bash
App                                       ContainerID      Pid
main:org.dde.calendar/5.14.5.0/x86_64     c3b5ce363172     539537
```

2. 进入`the org.dde.calendar`容器内部：

```bash
ll-cli exec main:org.dde.calendar/5.14.5.0/x86_64  /bin/bash
```

`ls -l /` 查看根目录结构，输出如下：

```text
lrwxrwxrwx   1 root   root        8  7月 27 20:58 bin -> /usr/bin
drwxr-xr-x   7 root   root      420  7月 27 20:58 dev
drwxr-xr-x 119 nobody nogroup 12288  7月 27 20:37 etc
drwxr-xr-x   3 root   root       60  7月 27 20:58 home
lrwxrwxrwx   1 root   root        8  7月 27 20:58 lib -> /usr/lib
lrwxrwxrwx   1 root   root       10  7月 27 20:58 lib32 -> /usr/lib32
lrwxrwxrwx   1 root   root       10  7月 27 20:58 lib64 -> /usr/lib64
lrwxrwxrwx   1 root   root       11  7月 27 20:58 libx32 -> /usr/libx32
drwxr-xr-x   2 root   root       40  7月 27 20:58 ll-host
drwxr-xr-x   3 root   root       60  7月 27 20:58 opt
dr-xr-xr-x 365 nobody nogroup     0  7月 27 20:58 proc
drwxr-xr-x   6 root   root      120  7月 27 20:58 run
drwxr-xr-x   7 nobody nogroup  4096  1月  1  1970 runtime
dr-xr-xr-x  13 nobody nogroup     0  7月 21 16:43 sys
drwxr-xr-x   3 root   root     4096  7月 27 20:58 tmp
drwxr-xr-x  15 nobody nogroup  4096  6月 30 16:22 usr
drwxr-xr-x  13 nobody nogroup  4096  6月 30 16:23 var
```
