
## ll-cli list

```bash
ll-cli list --help
```

```plain
Usage: ll-cli [options] list

Options:
  -h, --help                           Displays this help.
  --default-config                     default config json filepath
  --nodbus                             execute cmd directly, not via dbus
  --type <--type=installed>            query installed app
  --repo-point <--repo-point=flatpak>  app repo type to use

Arguments:
  list                                 show installed application
```
要查看已安装的runtime和应用，运行list命令：

```bash
ll-cli list
```