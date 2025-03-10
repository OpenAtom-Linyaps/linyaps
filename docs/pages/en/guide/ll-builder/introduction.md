<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# ll-builder Introduction

`ll-builder` is a tool for developers for building linyaps applications.

The main functions are as follows:

- Supports building in a standalone sandbox.

<!-- - Defined a version management system. -->

- Provides DTK software development kits.

<!-- - Contains a complete release process. -->

View the help information for the `ll-builder` command:

```bash
ll-builder --help
```

Here is the output:

```text
linyaps builder CLI
A CLI program to build linyaps application

Usage: ll-builder [OPTIONS] [SUBCOMMAND]

Options:
  -h,--help                   Print this help message and exit
  --help-all                  Expand all help
  --version                   Show version

Subcommands:
  create                      Create linyaps build template project
  build                       Build a linyaps project
  run                         Run builded linyaps app
  export                      Export to linyaps layer or uab
  push                        Push linyaps app to remote repo
  import                      Import linyaps layer to build repo
  extract                     Extract linyaps layer to dir
  repo                        Display and manage repositories

If you found any problems during use
You can report bugs to the linyaps team under this project: https://github.com/OpenAtom-Linyaps/linyaps/issues
```
