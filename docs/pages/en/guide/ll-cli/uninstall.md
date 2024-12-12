<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# Uninstall App

Use `ll-cli uninstall` to uninstall linyaps apps.

View the help information for the `ll-cli uninstall` command:

```bash
ll-cli uninstall --help
```

Here is the output:

```text
Uninstall the application or runtimes
Usage: ll-cli uninstall [OPTIONS] APP

Positionals:
  APP TEXT REQUIRED           Specify the applications ID

Options:
  -h,--help                   Print this help message and exit
  --help-all                  Expand all help
  --module MODULE             Uninstall a specify module

If you found any problems during use,
You can report bugs to the linyaps team under this project: https://github.com/OpenAtom-Linyaps/linyaps/issues
```

The command below gives an example of how to uninstall a linyaps app:

```bash
ll-cli uninstall <org.deepin.calculator>
```

Here is the output of `ll-cli uninstall org.deepin.calculator`:

```text
Uninstall main:org.deepin.calculator/5.7.21.4/x86_64 success:100%
```

After the command is executed successfully, the org.deepin.calculator will be uninstalled.
