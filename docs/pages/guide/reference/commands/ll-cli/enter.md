% ll-cli-exec 1

## NAME

ll\-cli\-exec - 在当前运行的沙盒中执行命令

## SYNOPSIS

**ll-cli enter** [*options*] _instance_ _command_...

## DESCRIPTION

`ll-cli enter` 命令可以进入正在运行的容器内部。该命令允许用户在正在运行的应用程序容器中执行指定的命令。

## OPTIONS

**-h, --help**
: 打印帮助信息并退出

**--help-all**
: 展开所有帮助

**--working-directory** _PATH:DIR_
: 指定工作目录

## POSITIONAL ARGUMENTS

**INSTANCE** _TEXT_ _REQUIRED_
: 指定正在运行的应用程序实例（可以通过 ps 命令获取）

**COMMAND** _TEXT_...
: 在正在运行的沙盒中运行命令

## EXAMPLES

使用 `ll-cli enter` 进入正在运行的容器内部：

1. 使用 `ll-cli ps` 获取正在运行的容器ID:

```bash
App                                       ContainerID      Pid
main:org.dde.calendar/5.14.5.0/x86_64     c3b5ce363172     539537
```

2. 进入 `org.dde.calendar` 容器内部：

```bash
ll-cli enter main:org.dde.calendar/5.14.5.0/x86_64 /bin/bash
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

## SEE ALSO

**[ll-cli(1)](./ll-cli.md)**, **[ll-cli-ps(1)](ps.md)**, **[ll-cli-run(1)](run.md)**

## HISTORY

2023年，由 UnionTech Software Technology Co., Ltd. 开发
