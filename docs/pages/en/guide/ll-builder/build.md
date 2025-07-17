<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# Build App

Use `ll-builder build` to build a linyaps application.

View the help information for the `ll-builder build` command:

```bash
ll-builder build --help
```

Here is the output:

```text
Build a linyaps project
Usage: ll-builder build [OPTIONS] [COMMAND...]

Positionals:
  COMMAND TEXT ...            Enter the container to execute command instead of building applications

Options:
  -h,--help                   Print this help message and exit
  --help-all                  Expand all help
  --file FILE:FILE [./linglong.yaml]
                              File path of the linglong.yaml
  --arch ARCH                 Set the build arch
  --offline                   Only use local files. This implies --skip-fetch-source and --skip-pull-depend will be set
  --skip-fetch-source         Skip fetch sources
  --skip-pull-depend          Skip pull dependency
  --skip-run-container        Skip run container
  --skip-commit-output        Skip commit build output
  --skip-output-check         Skip output check
  --skip-strip-symbols        Skip strip debug symbols
```

The `ll-builder build` command can be run in two ways:

1. the root directory of the project, where the `linglong.yaml` file is located.
2. specify the linglong.yaml file path with the `--file` parameter.

Taking the linyaps project `org.deepin.demo`, as an example, the main steps to build a linyaps application would be as follows:

Go to the `org.deepin.demo` project directory:

```bash
cd org.deepin.demo
```

Execute the `ll-builder build` command to start building:

```bash
ll-builder build
```

After the build is complete, the build content will be automatically committed to the local ostree cache. See `ll-builder export` for exporting build content.

Use the `--exec` parameter to enter the linyaps container before the build script is executed:

```bash
ll-builder build --exec /bin/bash
```

After entering the container, you can execute `shell` commands, such as `gdb`, `strace`, etc.

For more debugging information of linyaps application `debug` version, please refer to: [Debug App](../debug/debug.md).
