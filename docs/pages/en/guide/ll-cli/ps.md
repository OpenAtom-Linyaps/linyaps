<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# View Running Apps

Use `ll-cli ps` to view running Linglong Apps.

View the help information for the `ll-cli ps` command:

```bash
ll-cli ps --help
```

Here is the output:

```text
Usage: ll-cli [options] ps

Options:
  -h, --help                 Displays help on commandline options.
  --help-all                 Displays help including Qt specific options.
  --output-format <console>  json/console

Arguments:
  ps                         show running applications
```

Use `ll-cli ps` to view running Linglong Apps:

```bash
ll-cli ps
```

Here is the output of `ll-cli ps`:

```text
App                                             ContainerID                         Pid     Path
org.deepin.calculator/5.7.21.4/x86_64           7c4299db7f5647428a79896658efa35c    1943975 /run/user/1000/linglong/7c4299db7f5647428a79896658efa35c
```
