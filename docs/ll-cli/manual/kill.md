## ll-cli kill

```bash
ll-cli kill --help
```

```plain
Usage: ll-cli [options] kill container-id

Options:
  -h, --help        Displays this help.
  --default-config  default config json filepath
  --nodbus          execute cmd directly, not via dbus

Arguments:
  kill              kill container with id
  container-id      container id
```
通过kill命令可以强制退出正在运行的玲珑应用，如:

```bash
ll-cli kill <container-id>
```