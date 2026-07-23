% ll-cli-run 1

## NAME

ll\-cli\-run - 运行应用程序

## SYNOPSIS

**ll-cli run** [*options*] _app_ [*command*...]

## DESCRIPTION

`ll-cli run` 命令可以启动一个如意玲珑应用。该命令支持通过应用名运行应用程序，也可以在容器中执行命令而不是运行应用程序。

高级用户可以通过 [`ll-cli` 运行时配置](../../../extra/runtime_config.md)为全局或单个应用持久设置环境变量、挂载、设备和实例。

## OPTIONS

**-h, --help**
: 打印帮助信息并退出

**--help-all**
: 展开所有帮助

**--file** _FILES:FILE_...
: 将文件传递到沙盒中运行的应用程序

**--url** _URLS_...
: 将URL传递到沙盒中运行的应用程序

**--env** _ENV_...
: 为应用程序设置环境变量（格式：KEY=VALUE）

**--base** _REF_
: 指定应用程序运行使用的基础环境

**--runtime** _REF_
: 指定应用程序运行使用的运行时

**--workdir** _PATH_
: 指定应用程序的工作目录

**--extensions** _REF_...
: 指定应用程序运行使用的扩展（多个扩展用逗号分隔）

**--enable-xdp**, **--disable-xdp**
: 启用或禁用沙盒内的 xdg-desktop-portal 集成；同时指定时以最后一个选项为准

**--enable-pipewire**
: 将 PipeWire 套接字挂载到沙盒中

**--cdi-spec-dir** _DIR_...
: 指定 CDI 规范目录，默认为 `/etc/cdi,/var/run/cdi`，多个目录用逗号分隔

**--device** _DEVICE_...
: 添加 CDI 设备，多个设备用逗号分隔

**--device-mode** _MODE_...
: 指定设备模式；当前支持 `passthru`，多个模式用逗号分隔

**--instance** _NAME_
: 指定容器实例名称，以便复用或标识实例

**--debug**
: 使用 gdbserver 启动应用

**--debug-listen** _ADDR_ [*:2345*]
: 指定 gdbserver 监听地址；必须与 `--debug` 一起使用

**--debug-debuginfod** _URLS_
: 指定调试时使用的 debuginfod URL；必须与 `--debug` 一起使用

**--debug-symbol-dir** _DIR_
: 指定 GDB 加载调试符号的目录；必须与 `--debug` 一起使用

## POSITIONAL ARGUMENTS

**APP** _TEXT_ _REQUIRED_
: 指定应用程序名

**COMMAND** _TEXT_...
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

**[ll-cli(1)](./ll-cli.md)**, **[ll-cli-ps(1)](./ps.md)**, **[ll-cli-enter(1)](./enter.md)**

## HISTORY

2023年，由 UnionTech Software Technology Co., Ltd. 开发
