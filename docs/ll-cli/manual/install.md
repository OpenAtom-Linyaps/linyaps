## ll-cli install

`ll-cli install`命令用来安装玲珑应用。

查看`ll-cli install`命令的帮助信息：

```bash
ll-cli install --help
```

`ll-cli install`命令的帮助信息如下：

```plain
Usage: ll-cli [options] install com.deepin.demo

Options:
  -h, --help                           Displays this help.
  --default-config                     default config json filepath
  --repo-point <--repo-point=flatpak>  app repo type to use
  --nodbus                             execute cmd directly, not via dbus

Arguments:
  install                              install an application
  appId                                application id
```

运行`ll-cli install`命令安装玲珑应用:

```bash
ll-cli install <org.deepin.music>
```

`ll-cli install`命令需要输入应用完整的`appid`，若仓库有多个版本则会默认安装最高版本。

安装指定版本需在`appid`后附加对应版本号:

```bash
ll-cli install <org.deepin.music/5.1.2>
```

应用安装完成后，客户端会显示安装结果信息。
