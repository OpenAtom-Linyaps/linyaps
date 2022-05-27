## ll-cli run

`ll-cli run`命令可以启动一个玲珑应用。

查看 `ll-cli run`命令的帮助信息：

```bash
ll-cli run --help
```

`ll-cli run`命令的帮助信息如下：

```plain
Usage: ll-cli [options] run com.deepin.demo

Options:
  -h, --help                                         Displays help on
                                                     commandline options.
  --help-all                                         Displays help including Qt
                                                     specific options.
  --default-config                                   default config json
                                                     filepath
  --repo-point <--repo-point=flatpak>                app repo type to use
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
ll-cli run <org.deepin.music>
```

默认情况下会使用`ll-dbus-proxy`拦截转发`dbus`消息，如果不想使用`ll-dbus-proxy`，可以使用`--no-proxy`参数：

```bash
ll-cli run <org.deepin.music> --no-proxy
```

使用 `ll-cli run`命令可以进入指定程序沙箱环境：

```bash
ll-cli run <org.deepin.music> --exec /bin/bash
```

进入后可执行 `shell` 命令，如`gdb`、`strace`、`ls`、`find`等。

由于玲珑应用都是在沙箱内运行，无法通过常规的方式直接调试，需要在沙箱内运行调试工具，如 `gdb`：

```bash
gdb /opt/apps/org.deepin.music/files/bin/deepin-music
```

该路径为沙箱内应用程序的绝对路径。

玲珑应用`release`版本更多调试信息请参考：[FAQ](faq.md)
