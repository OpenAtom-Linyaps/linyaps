<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# Run compiled App

Use `ll-builder run` to run the compiled executable program.

View the help information for the `ll-builder run` command:

```bash
ll-builder run --help
```

Here is the output:

```text
Run builded linyaps app
Usage: ll-builder run [OPTIONS] [COMMAND...]

Positionals:
  COMMAND TEXT ...            Enter the container to execute command instead of running application

Options:
  -h,--help                   Print this help message and exit
  --help-all                  Expand all help
  --file FILE:FILE [./linglong.yaml]
                              File path of the linglong.yaml
  --offline                   Only use local files
  --modules modules ...       Run specified module. eg: --modules binary,develop
  --debug                     Run in debug mode (enable develop module)
```

The `ll-builder run` command reads the operating system environment information related to the program according to the configuration file, constructs a container, and executes the program in the container without installation.

```bash
ll-builder run
```

If `ll-builder run` runs successfully, the output is as follows:

```bash
hello world
```

To facilitate debugging, use an additional `--exec /bin/bash` parameter to replace the default execution program after entering the container, such as:

```bash
ll-builder run --exec /bin/bash
```

With this option, `ll-builder` will enter the `bash` terminal after creating the container, and can perform other operations inside the container.
