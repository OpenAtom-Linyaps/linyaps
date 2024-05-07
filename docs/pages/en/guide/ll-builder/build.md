<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# Build App

Use `ll-builder build` to build a Linglong application.

View the help information for the `ll-builder build` command:

```bash
ll-builder build --help
```

Here is the output:

```text
Usage: ll-builder [options] build
linglong build command tools
Examples:
ll-builder build -v
ll-builder build -v -- bash -c "echo hello"

Options:
  -v, --verbose         show detail log (deprecated, use QT_LOGGING_RULES)
  -h, --help            Displays help on commandline options.
  --help-all            Displays help including Qt specific options.
  --exec <command>      run exec than build script
  --offline             only use local files. This implies --skip-fetch-source
                        and --skip-pull-depend
  --skip-fetch-source   skip fetch sources
  --skip-pull-depend    skip pull dependency
  --skip-run-container  skip run container. This implies skip-commit-output
  --skip-commit-output  skip commit build output
  --arch <arch>         set the build arch

Arguments:
  build                 build project
```

The `ll-builder build` command must be run in the root directory of the project, where the `linglong.yaml` file is located.

Taking the Linglong project `org.deepin.demo`, as an example, the main steps to build a Linglong application would be as follows:

Go to the `org.deepin.demo` project directory:

```bash
cd org.deepin.demo
```

Execute the `ll-builder build` command to start building:

```bash
ll-builder build
```

After the build is complete, the build content will be automatically committed to the local ostree cache. See `ll-builder export` for exporting build content.

Use the `--exec` parameter to enter the Linglong container before the build script is executed:

```bash
ll-builder build --exec /bin/bash
```

After entering the container, you can execute `shell` commands, such as `gdb`, `strace`, etc.

For more debugging information of Linglong application `debug` version, please refer to: [Debug App](../debug/debug.md).
