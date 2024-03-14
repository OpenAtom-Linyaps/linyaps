<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# 运行应用

`ll-cli run`命令可以启动一个玲珑应用。

查看 `ll-cli run`命令的帮助信息：

```bash
ll-cli run --help
```

`ll-cli run`命令的帮助信息如下：

```text
Usage: ll-cli [options] run com.deepin.demo

Options:
  -h, --help                                         Displays help on
                                                     commandline options.
  --help-all                                         Displays help including Qt
                                                     specific options.
  --repo-point                                       app repo type to use
  --exec </bin/bash>                                 run exec
  --no-proxy                                         whether to use dbus proxy
                                                     in box
  --filter-name <--filter-name=com.deepin.linglong.A dbus name filter to use
  ppManager>
  --filter-path <--filter-path=/com/deepin/linglong/ dbus path filter to use
  PackageManager>
  --filter-interface <--filter-interface=com.deepin. dbus interface filter to
  linglong.PackageManager>                           use

Arguments:
  run                                                run application
  appId                                              application id
```

当应用被正常安装后，使用`ll-cli run`命令即可启动：

```bash
ll-cli run <org.deepin.calculator>
```

默认情况下执行run命令会启动最高版本的应用，若需运行指定版本应用，需在`appid`后附加对应版本号：

```bash
ll-cli run <org.deepin.calculator/5.7.21.4>
```

默认情况下会使用`ll-dbus-proxy`拦截转发`dbus`消息，如果不想使用`ll-dbus-proxy`，可以使用`--no-proxy`参数：

```bash
ll-cli run <org.deepin.calculator> --no-proxy
```

如果你想传递文件参数给app, 你能使用`--file`选项, 然后在命令行参数中, 你能使用%%f将参数传递到指定的容器中运行, 文件的路径将映射到容器的`/run/host`目录下
```bash
ll-cli run <org.deepin.editor> --file /home/deepin/Desktop/test.txt -- %%f
```

如果你想传递URL参数给app, 你能使用`--url`选项, 然后在命令行参数中, 你能使用%%u将参数传递到指定的容器中运行, 如果是`file:///`类型的URL, 文件的路径将映射到容器的`/run/host`目录下
```bash
ll-cli run <org.deepin.editor> --url file::////home/deepin/Desktop/test.txt -- %%u
```

使用 `ll-cli run`命令可以进入指定程序容器环境：

```bash
ll-cli run <org.deepin.calculator> --exec /bin/bash
```

进入后可执行 `shell` 命令，如`gdb`、`strace`、`ls`、`find`等。

由于玲珑应用都是在容器内运行，无法通过常规的方式直接调试，需要在容器内运行调试工具，如 `gdb`：

```bash
gdb /opt/apps/org.deepin.calculator/files/bin/deepin-calculator
```

该路径为容器内应用程序的绝对路径。

玲珑应用`release`版本更多调试信息请参考：[常见运行问题](../debug/faq.md)。
