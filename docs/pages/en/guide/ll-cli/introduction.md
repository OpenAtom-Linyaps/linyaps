<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# ll-cli Introduction

`ll-cli` is the command line tool of linyaps, used to install, uninstall, check, run, close, debug, and update linyaps applications.

View the help information for the `ll-cli` command:

```bash
ll-cli --help
```

Here is the output:

```text
Run an application
Usage: ll-cli run [OPTIONS] APP [COMMAND...]

Example:
# run application by appid
ll-cli run org.deepin.demo
# execute commands in the container rather than running the application
ll-cli run org.deepin.demo bash
ll-cli run org.deepin.demo -- bash
ll-cli run org.deepin.demo -- bash -x /path/to/bash/script

Positionals:
  APP TEXT REQUIRED           Specify the application ID
  COMMAND TEXT ...            Run commands in a running sandbox

Options:
  -h,--help                   Print this help message and exit
  --help-all                  Expand all help
  --file FILE:FILE            Pass file to applications running in a sandbox
  --url URL                   Pass url to applications running in a sandbox

If you found any problems during use,
You can report bugs to the linyaps team under this project: https://github.com/OpenAtom-Linyaps/linyaps/issues
```
