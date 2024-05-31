<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# ll-builder Introduction

`ll-builder` is a tool for developers for building Linglong applications.

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
Usage: ll-builder [options] subcommand [sub-option]

Options:
  -v, --verbose  show detail log
  -h, --help     Displays help on commandline options.
  --help-all     Displays help including Qt specific options.

Arguments:
  subcommand     create
                 build
                 run
                 export
                 push
                 convert
                 import
                 extract
```
