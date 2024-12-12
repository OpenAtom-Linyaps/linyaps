<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# Force Quit Running App

Use `ll-cli kill` to force quit running linyaps apps.

View the help information for the `ll-cli kill` command:

```bash
ll-cli kill --help
```

Here is the output:

```text
Stop running applications
Usage: ll-cli kill [OPTIONS] APP

Positionals:
  APP TEXT REQUIRED           Specify the running application

Options:
  -h,--help                   Print this help message and exit
  --help-all                  Expand all help

If you found any problems during use,
You can report bugs to the linyaps team under this project: https://github.com/OpenAtom-Linyaps/linyaps/issues
```

Example of the `ll-cli kill` command to force quit running linyaps apps:

```bash
ll-cli kill  org.deepin.calculator
```
