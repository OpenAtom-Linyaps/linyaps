## ll-cli uninstall

`ll-cli uninstall`命令可以安装玲珑应用。

查看`ll-cli uninstall`命令的帮助信息：

```bash
ll-cli uninstall --help
```

`ll-cli uninstall`命令的帮助信息如下：

```plain
Usage: ll-cli [options] uninstall com.deepin.demo

Options:
  -h, --help                           Displays this help.
  --default-config                     default config json filepath
  --nodbus                             execute cmd directly, not via dbus
  --repo-point <--repo-point=flatpak>  app repo type to use

Arguments:
  uninstall                            uninstall an application
  appId                                app id
```

使用`ll-cli uninstall`命令卸载玲珑应用：

```bash
ll-cli uninstall <org.deepin.music>
```

默认卸载最高版本，可以通过`appid`后附加对应版本号卸载指定版本：

```bash
ll-cli uninstall <org.deepin.music/5.1.2>
```

该命令执行成功后，该玲珑应用将从系统中被卸载掉。
