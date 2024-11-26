<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# Update linyaps App

Use `ll-cli upgrade` to upgrade linyaps apps.

View the help information for the `ll-cli upgrade` command:

```bash
ll-cli upgrade --help
```

Here is the output:

```text
Upgrade the application or runtimes
Usage: ll-cli upgrade [OPTIONS] [APP]

Positionals:
  APP TEXT                    Specify the application ID.If it not be specified, all applications will be upgraded

Options:
  -h,--help                   Print this help message and exit
  --help-all                  Expand all help

If you found any problems during use,                                                                                                                                                           You can report bugs to the linyaps team under this project: https://github.com/OpenAtom-Linyaps/linyaps/issues
```

Use `ll-cli upgrade ` to upgrade all installed apps to the latest version

Here is the output:

```text
Upgrade main:org.dde.calendar/5.14.5.0/x86_64 to main:org.dde.calendar/5.14.5.1/x86_64 success:100%
Upgrade main:org.deepin.mail/6.4.10.0/x86_64 to main:org.deepin.mail/6.4.10.1/x86_64 success:100%
Please restart the application after saving the data to experience the new version.
```

Use `ll-cli upgrade org.deepin.calculator` to upgrade org.deepin.calculator to the latest version in the remote repository, such as:

```bash
ll-cli upgrade org.deepin.calculator
```

Here is the output:

```text
Upgrade main:org.deepin.calculator/5.7.21.3/x86_64 to main:org.deepin.calculator/5.7.21.4/x86_64 success:100%
Please restart the application after saving the data to experience the new version.
```


