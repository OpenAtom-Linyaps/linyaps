## ll-cli update

```bash
ll-cli update --help
```

```plain
Usage: ll-cli [options] update com.deepin.demo

Options:
  -h, --help        Displays this help.
  --default-config  default config json filepath
  --nodbus          execute cmd directly, not via dbus

Arguments:
  update            update an application
  appId             app id
```

通过update命令将本地软件包版本更新到远端仓库中的最新版本，如:

```bash
ll-cli update <org.deepin.music>
```

更新特定版本的软件包

```bash
ll-cli update <org.deepin.calculator/5.5.23>
```