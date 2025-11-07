% ll-cli-run 1

## NAME

ll\-cli\-run - 运行应用程序

## SYNOPSIS

**ll-cli run** [*options*] *app* [*command*...]

## DESCRIPTION

`ll-cli run` 命令可以启动一个如意玲珑应用。该命令支持通过应用名运行应用程序，也可以在容器中执行命令而不是运行应用程序。

## OPTIONS

**-h, --help**
: 打印帮助信息并退出

**--help-all**
: 展开所有帮助

**--file** *FILES:FILE*...
: 将文件传递到沙盒中运行的应用程序

**--url** *URLS*...
: 将URL传递到沙盒中运行的应用程序

**--env** *ENV*...
: 为应用程序设置环境变量（格式：KEY=VALUE）

**--base** *REF*
: 指定应用程序运行使用的基础环境

**--runtime** *REF*
: 指定应用程序运行使用的运行时

**--extensions** *REF*...
: 指定应用程序运行使用的扩展（多个扩展用逗号分隔）

## POSITIONAL ARGUMENTS

**APP** *TEXT* *REQUIRED*
: 指定应用程序名

**COMMAND** *TEXT*...
: 在正在运行的沙盒中运行命令

## EXAMPLES

通过应用名运行应用程序：

```bash
ll-cli run org.deepin.demo
```

在容器中执行命令而不是运行应用程序：

```bash
ll-cli run org.deepin.demo bash
ll-cli run org.deepin.demo -- bash
ll-cli run org.deepin.demo -- bash -x /path/to/bash/script
```

## SEE ALSO

**[ll-cli(1)](./ll-cli.md)**, **[ll-cli-ps(1)](./ps.md)**, **[ll-cli-exec(1)](./enter.md)**

## HISTORY

2023年，由 UnionTech Software Technology Co., Ltd. 开发
