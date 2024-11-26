<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# Run App

Use `ll-cli run` command to start a linyaps application.

See help for the `ll-cli run` command:

```bash
ll-cli run --help
```

View the help information for the `ll-cli run` command:

```text
Run an application
Usage: ll-cli run [OPTIONS] APP [COMMAND...]

Example:
# run application by appid                                                                                                                                                                      ll-cli run org.deepin.demo
# execute commands in the container rather than running the application
ll-cli run org.deepin.demo bash
ll-cli run org.deepin.demo -- bash
ll-cli run org.deepin.demo -- bash -x /path/to/bash/script

Positionals:
  APP TEXT REQUIRED           Specify the application ID
  COMMAND TEXT ...            Run commands in a running sandbox

Options:
  -h,--help                   Print this help message and exit
  --help-all                  Expand all help                                                                                                                                                     --file FILE:FILE            Pass file to applications running in a sandbox
  --url URL                   Pass url to applications running in a sandbox

If you found any problems during use,
You can report bugs to the linyaps team under this project: https://github.com/OpenAtom-Linyaps/linyaps/issues
```

When the application is installed normally, use the `ll-cli run` command to start it:

```bash
ll-cli run org.deepin.calculator
```

Use the `ll-cli run` command to enter the specified program container:

```bash
ll-cli run org.deepin.calculator --exec /bin/bash
```

After entering, execute `shell` commands, such as `gdb`, `strace`, `ls`, `find`, etc.

Since linyaps applications run in the container, they cannot be directly debugged in the conventional way. You need to run debugging tools in the container, such as `gdb`:

```bash
gdb /opt/apps/org.deepin.calculator/files/bin/deepin-calculator
```

The path is the absolute path of the application in the container.

For more debugging information on linyaps application `release` version, please refer to: [Run FAQ](../debug/faq.md).
