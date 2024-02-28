<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# Update Linglong App

Use `ll-cli update` to update Linglong apps.

View the help information for the `ll-cli update` command:

```bash
ll-cli update --help
```

Here is the output:

```text
Usage: ll-cli [options] update com.deepin.demo

Options:
  -h, --help                      Displays help on commandline options.
  --help-all                      Displays help including Qt specific options.
  --channel <--channel=linglong>  the channel of app
  --module <--module=runtime>     the module of app

Arguments:
  update                          update an application
  appId                           application id
```

Use `ll-cli update` to update a local app to the latest version in the remote repository, such as:

```bash
ll-cli update <org.deepin.calculator>
```

Here is the output:

```text
update org.deepin.calculator , please wait a few minutes...
org.deepin.calculator is updating...
message: update org.deepin.calculator success, version:5.7.16 --> 5.7.21.4
```

Example of updating the specified version to the latest version:

```bash
ll-cli update <org.deepin.calculator/5.7.16>
```
