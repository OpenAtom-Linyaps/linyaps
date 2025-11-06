% ll-cli-kill 1

## NAME

ll\-cli\-kill - 停止运行的应用程序

## SYNOPSIS

**ll-cli kill** [*options*] _app_

## DESCRIPTION

`ll-cli kill` 命令可以强制退出正在运行的玲珑应用。该命令用于终止正在运行的应用程序进程。

## OPTIONS

**-h, --help**
: 打印帮助信息并退出

**--help-all**
: 展开所有帮助

**-s, --signal** _SIGNAL_ [*SIGTERM*]
: 指定发送给应用程序的信号

## POSITIONAL ARGUMENTS

**APP** _TEXT_ _REQUIRED_
: 指定正在运行的应用程序名

## EXAMPLES

使用 `ll-cli kill` 命令可以强制退出正在运行的玲珑应用:

```bash
ll-cli kill org.deepin.calculator
```

## SEE ALSO

**[ll-cli(1)](./ll-cli.md)**, **[ll-cli-ps(1)](ps.md)**, **[ll-cli-run(1)](run.md)**

## HISTORY

2023年，由 UnionTech Software Technology Co., Ltd. 开发
