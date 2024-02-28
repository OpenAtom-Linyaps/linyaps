<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# Query Apps From Remote

Use `ll-cli query` to search app meta info from remote repository.

View the help information for the `ll-cli query` command:

```bash
ll-cli query --help
```

Here is the output:

```text
Usage: ll-cli [options] query com.deepin.demo

Options:
  -h, --help                           Displays help on commandline options.
  --help-all                           Displays help including Qt specific
                                       options.
  --repo-point <--repo-point=flatpak>  app repo type to use
  --force                              query from server directly, not from
                                       cache

Arguments:
  query                                query app info
  appId                                application id
```

Use `ll-cli query` to search app meta info from remote repository and local cache:

```bash
ll-cli query <calculator>
```

Add `--force` to force search app info from remote repository:

```bash
ll-cli query <calculator> --force
```

This command returns the info of all apps whose `appid` (appid is the app's unique identifier) contains the keyword "calculator", including the complete `appid`, application name, version, CPU architecture and descriptions.

Here is the output:

```text
appId                           name                            version         arch        channel         module      description
org.deepin.calculator           deepin-calculator               5.5.23          x86_64      linglong        runtime     Calculator for UOS
org.deepin.calculator           deepin-calculator               5.7.1           x86_64      linglong        runtime     Calculator for UOS
```
