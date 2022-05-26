## ll-cli uninstall

```bash
ll-cli uninstall --help
```

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

卸载应用程序，使用uninstall命令：

```bash
ll-cli uninstall <org.deepin.music>
```

该命令执行成功后，应用程序将从系统中被移除，但并不包含用户使用该应用过程中产生的数据信息。