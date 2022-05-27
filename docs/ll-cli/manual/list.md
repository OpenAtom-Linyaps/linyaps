## ll-cli list

`ll-cli list`命令可以查看已安装的玲珑应用。

查看 `ll-cli list`命令的帮助信息：

```bash
ll-cli list --help
```

`ll-cli list`命令的帮助信息如下：

```plain
Usage: ll-cli [options] list

Options:
  -h, --help                           Displays this help.
  --default-config                     default config json filepath
  --type <--type=installed>            query installed app
  --repo-point <--repo-point=flatpak>  app repo type to use

Arguments:
  list                                 show installed application
```

查看已安装的`runtime` 和应用，运行 `ll-cli list`命令：

```bash
ll-cli list
```
