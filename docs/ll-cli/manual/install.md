## ll-cli install

```bash
ll-cli install --help
```

```plain
Usage: ll-cli [options] install com.deepin.demo

Options:
  -h, --help                           Displays this help.
  --default-config                     default config json filepath
  --nodbus                             execute cmd directly, not via dbus
  --repo-point <--repo-point=flatpak>  app repo type to use

Arguments:
  install                              install an application
  appId                                app id
```

要安装应用，请运行install命令，如：

```bash
ll-cli install org.deepin.music
```
install命令需要输入应用完整的appid，若仓库有多个版本则会默认安装最高版本；指定版本安装需在appid后附加对应版本号，如：
```bash
ll-cli install org.deepin.music/5.1.2
```

应用安装完成后，客户端会显示安装结果信息。