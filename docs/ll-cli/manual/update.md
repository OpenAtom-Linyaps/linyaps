## ll-cli update

`ll-cli update`命令可以更新玲珑应用。

查看`ll-cli update`命令的帮助信息：

```bash
ll-cli update --help
```

`ll-cli update`命令的帮助信息如下：

```plain
Usage: ll-cli [options] update com.deepin.demo

Options:
  -h, --help        Displays this help.
  --default-config  default config json filepath

Arguments:
  update            update an application
  appId             application id
```

通过`ll-cli update`命令将本地软件包版本更新到远端仓库中的最新版本，如:

```bash
ll-cli update <org.deepin.music>
```

更新指定版本的软件包

```bash
ll-cli update <org.deepin.calculator/5.5.23>
```
