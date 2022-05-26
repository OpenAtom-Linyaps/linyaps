## ll-cli exec

```bash
ll-cli exec --help
```

```plain
Usage: ll-cli [options] aebbe2f455cf443f89d5c92f36d154dd /bin/bash

Options:
  -h, --help        Displays this help.
  --default-config  default config json filepath
  --nodbus          execute cmd directly, not via dbus

Arguments:
  containerId       container id
  exec              exec command in container
```

使用exec进入正在运行的容器内部：

```bash
ll-cli exec aebbe2f455cf443f89d5c92f36d154dd /bin/bash
```