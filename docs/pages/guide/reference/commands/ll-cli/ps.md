% ll-cli-ps 1

## NAME

ll\-cli\-ps - 列出正在运行的应用程序

## SYNOPSIS

**ll-cli ps** [*options*]

## DESCRIPTION

`ll-cli ps` 命令可以查看正在运行的如意玲珑应用。该命令显示当前系统中所有正在运行的玲珑应用程序及其相关信息。

## OPTIONS

**-h, --help**
: 打印帮助信息并退出

**--help-all**
: 展开所有帮助

## EXAMPLES

查看正在运行的应用，运行 `ll-cli ps` 命令：

```bash
ll-cli ps
```

`ll-cli ps` 命令输出如下：

```text
应用              容器ID        进程ID
org.dde.calendar  4e3407a8a052  1279610
```

## SEE ALSO

**[ll-cli(1)](./ll-cli.md)**, **[ll-cli-run(1)](run.md)**, **[ll-cli-kill(1)](kill.md)**

## HISTORY

2023年，由 UnionTech Software Technology Co., Ltd. 开发
