<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# Run App

Use `ll-cli run` command to start a Linglong application.

See help for the `ll-cli run` command:

```bash
ll-cli run --help
```

View the help information for the `ll-cli run` command:

```text
Usage: ll-cli [options] run com.deepin.demo

Options:
  -h, --help                                         Displays help on
                                                     commandline options.
  --help-all                                         Displays help including Qt
                                                     specific options.
  --repo-point                                       app repo type to use
  --exec </bin/bash>                                 run exec
  --no-proxy                                         whether to use dbus proxy
                                                     in box
  --filter-name <--filter-name=com.deepin.linglong.A dbus name filter to use
  ppManager>
  --filter-path <--filter-path=/com/deepin/linglong/ dbus path filter to use
  PackageManager>
  --filter-interface <--filter-interface=com.deepin. dbus interface filter to
  linglong.PackageManager>                           use

Arguments:
  run                                                run application
  appId                                              application id
```

When the application is installed normally, use the `ll-cli run` command to start it:

```bash
ll-cli run <org.deepin.calculator>
```

By default, executing the run command will start the application of the highest version. If you want to run the application of the specified version, you need to append the corresponding version number after `appid`:

```bash
ll-cli run <org.deepin.calculator/5.7.21.4>
```

By default, `ll-dbus-proxy` is used to intercept and forward `dbus` messages. If you do not want to use `ll-dbus-proxy`, you can use the `--no-proxy` parameter:

```bash
ll-cli run <org.deepin.calculator> --no-proxy
```

Use the `ll-cli run` command to enter the specified program container:

```bash
ll-cli run <org.deepin.calculator> --exec /bin/bash
```

After entering, execute `shell` commands, such as `gdb`, `strace`, `ls`, `find`, etc.

Since Linglong applications run in the container, they cannot be directly debugged in the conventional way. You need to run debugging tools in the container, such as `gdb`:

```bash
gdb /opt/apps/org.deepin.calculator/files/bin/deepin-calculator
```

The path is the absolute path of the application in the container.

For more debugging information on Linglong application `release` version, please refer to: [Run FAQ](../debug/faq.md).
