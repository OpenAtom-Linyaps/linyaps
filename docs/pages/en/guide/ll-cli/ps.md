<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# View Running Apps

Use `ll-cli ps` to view running linyaps Apps.

View the help information for the `ll-cli ps` command:

```bash
ll-cli ps --help
```

Here is the output:

```text
List running applications
Usage: ll-cli ps [OPTIONS]

Options:
  -h,--help                   Print this help message and exit
  --help-all                  Expand all help

If you found any problems during use,
You can report bugs to the linyaps team under this project: https://github.com/OpenAtom-Linyaps/linyaps/issues
```

Use `ll-cli ps` to view running linyaps Apps:

```bash
ll-cli ps
```

Here is the output of `ll-cli ps`:

```text
App                                       ContainerID      Pid
main:org.dde.calendar/5.14.5.0/x86_64     c3b5ce363172     539537
```
