## ll-cli ps

```bash
ll-cli ps --help
```

```plain
Usage: ll-cli [options] ps

Options:
  -h, --help                 Displays this help.
  --default-config           default config json filepath
  --nodbus                   execute cmd directly, not via dbus
  --output-format <console>  json/console

Arguments:
  ps                         show running applications
```
查看正在运行的应用，运行ps命令：

```bash
ll-cli ps
```