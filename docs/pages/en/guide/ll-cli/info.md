<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# Display app information

Use `ll-cli info` to display information about installed apps or runtimes.

View the help information for the `ll-cli info` command:

```bash
ll-cli info --help
```

Here is the output:

```text
Display information about installed apps or runtimes
Usage: ll-cli info [OPTIONS] APP

Positionals:
  APP TEXT REQUIRED           Specify the application ID, and it can also be a .layer file

Options:
  -h,--help                   Print this help message and exit
  --help-all                  Expand all help

If you found any problems during use,
You can report bugs to the linyaps team under this project: https://github.com/OpenAtom-Linyaps/linyaps/issues
```

Use `ll-cli info org.dde.calendar` to display information about org.dde.calendar.

Here is the output:

```text
{
    "arch": [
        "x86_64"
    ],
    "base": "main:org.deepin.base/23.1.0/x86_64",
    "channel": "main",
    "command": [
        "dde-calendar"
    ],
    "description": "calendar for deepin os.\n",
    "id": "org.dde.calendar",
    "kind": "app",
    "module": "binary",
    "name": "dde-calendar",
    "runtime": "main:org.deepin.runtime.dtk/23.1.0/x86_64",
    "schema_version": "1.0",
    "size": 13483249,
    "version": "5.14.5.1"
}
```
