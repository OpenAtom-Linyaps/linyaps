## ll-cli run

```bash
ll-cli run --help
```

```plain
Usage: ll-cli [options] run com.deepin.demo

Options:
  -h, --help                           Displays help on commandline options.
  --help-all                           Displays help including Qt specific
                                       options.
  --default-config                     default config json filepath
  --nodbus                             execute cmd directly, not via dbus
  --repo-point <--repo-point=flatpak>  app repo type to use
  --exec </bin/bash>                   run exec

Arguments:
  run                                  run application
  appId                                application id
```

当应用被正常安装后，使用run命令及appid即可启动：

```bash
ll-cli run <org.deepin.music>
```

使用appid为指定程序创建一个沙箱，并获取到沙箱内的shell：
```bash
ll-cli run <org.deepin.music> --exec /bin/bash
```

由于linglong应用都是在沙箱内运行，无法通过常规的方式直接调试，需要在沙箱内运行调试工具，如gdb：

```bash
gdb /opt/apps/org.deepin.music/files/bin/deepin-music
```
该路径为沙箱内应用程序的绝对路径。