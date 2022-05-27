## ll-cli query

`ll-cli query`命令可以查询玲珑远程仓库中的应用信息。

查看`ll-cli query`命令的帮助信息：

```bash
ll-cli query --help
```

`ll-cli query`命令的帮助信息如下：

```plain
Usage: ll-cli [options] query com.deepin.demo

Options:
  -h, --help                           Displays this help.
  --default-config                     default config json filepath
  --repo-point <--repo-point=flatpak>  app repo type to use
  --force                              query from server directly, not from
                                       cache

Arguments:
  query                                query app info
  appId                                app id
```

通过`ll-cli query`命令可以从远程 repo 中找到应用程序信息:

```bash
ll-cli query <music>
```

加上`--force`可以强制从远程 `repo` 中查询应用信息:

```bash
ll-cli query <music> --force
```

该命令将返回`appid`(appid 是应用唯一标识) 中包含 music 关键词的所有应用程序信息，包含完整的`appid`、应用程序名称、版本、平台及应用描述信息。
