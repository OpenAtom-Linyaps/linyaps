<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# Search Apps From Remote

Use `ll-cli search` to search app meta info from remote repository.

View the help information for the `ll-cli search` command:

```bash
ll-cli search --help
```

Here is the output:

```text
Usage: ll-cli [options] search com.deepin.demo

Options:
  -h, --help                           Displays help on commandline options.
  --help-all                           Displays help including Qt specific
                                       options.
  --repo-point <--repo-point=flatpak>  app repo type to use

Arguments:
  query                                query app info
  appId                                application id
```

Use `ll-cli search` to search app meta info from remote repository and local cache:

```bash
ll-cli search calculator
```

This command returns the info of all apps whose `appid` (appid is the app's unique identifier) contains the keyword "calculator", including the complete `appid`, application name, version, CPU architecture and descriptions.

Here is the output:

```text
appId                           name                            version         arch        channel         module      description
org.deepin.calculator           deepin-calculator               5.5.23          x86_64      linglong        runtime     Calculator for UOS
org.deepin.calculator           deepin-calculator               5.7.1           x86_64      linglong        runtime     Calculator for UOS
```

If you need to look up Base and Runtime, you can use the following command:

```bash
ll-cli search . --type=runtime
```
