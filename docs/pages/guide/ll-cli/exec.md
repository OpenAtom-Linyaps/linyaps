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
Usage: ll-cli [options] exec 9c41c0af2bad4617aea8485f5aaeb93a "bash"

Options:
  -h, --help        Displays help on commandline options.
  --help-all        Displays help including Qt specific options.
  -e, --env <env>   extra environment variables splited by comma
  -d, --path <pwd>  location to exec the new command

Arguments:
  exec              exec command in container
  containerId       container id
  cmd               command
```

新开一个终端，执行如下命令：

```bash
killall ll-service
ll-service
```

使用`ll-cli exec`进入正在运行的容器内部：

```bash
ll-cli exec 9c41c0af2bad4617aea8485f5aaeb93a /bin/bash
```

返回`ll-service`终端界面，发现已进入应用容器。

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
