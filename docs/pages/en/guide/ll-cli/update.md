<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# Update Linglong App

Use `ll-cli upgrade` to upgrade Linglong apps.

View the help information for the `ll-cli upgrade` command:

```bash
ll-cli upgrade --help
```

Here is the output:

```text
Usage: ll-cli [options] upgrade com.deepin.demo

Options:
  -h, --help                      Displays help on commandline options.
  --help-all                      Displays help including Qt specific options.
  --channel <--channel=linglong>  the channel of app
  --module <--module=runtime>     the module of app

Arguments:
  upgrade                         update an application
  appId                           application id
```

Use `ll-cli upgrade` to upgrade a local app to the latest version in the remote repository, such as:

```bash
ll-cli upgrade org.deepin.calculator
```

Here is the output:

```text
100% Install main:org.deepin.calculator/5.7.21.4/x86_64 success
```

Example of updating the specified version to the latest version:

```bash
ll-cli upgrade org.deepin.calculator/5.7.16
```
