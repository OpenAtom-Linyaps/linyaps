## ll-cli query

```bash
ll-cli query --help
```

```plain
Usage: ll-cli [options] query com.deepin.demo

Options:
  -h, --help                           Displays this help.
  --default-config                     default config json filepath
  --nodbus                             execute cmd directly, not via dbus
  --repo-point <--repo-point=flatpak>  app repo type to use
  --force                              query from server directly, not from
                                       cache

Arguments:
  query                                query app info
  appId                                app id
```
通过query命令可以从远程repo中找到应用程序信息，如:

```bash
ll-cli query <music>
```
该命令将返回appid（appid是应用唯一标识）中包含music关键词的所有应用程序信息，包含完整的appid、应用程序名称、版本、平台及应用描述信息。