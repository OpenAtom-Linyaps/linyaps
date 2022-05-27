## ll-cli kill

`ll-cli kill`命令可以强制退出正在运行的玲珑应用。

查看`ll-cli kill`命令的帮助信息：

```bash
ll-cli kill --help
```

`ll-cli kill` 命令的帮助信息如下：

```plain
Usage: ll-cli [options] kill container-id

Options:
  -h, --help        Displays this help.
  --default-config  default config json filepath

Arguments:
  kill              kill container with id
  container-id      container id
```

使用 `ll-cli kill` 命令可以强制退出正在运行的玲珑应用:

```bash
ll-cli kill <aebbe2f455cf443f89d5c92f36d154dd>
```
