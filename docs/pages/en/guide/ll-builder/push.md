<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# Push UAB to Remote Repositories

Use the `ll-builder push` command to push Linglong packages to Linglong remote repositories.

View the help information for the `ll-builder push` command:

```bash
ll-builder push --help
```

Here is the output:

```text
Usage: ll-builder [options] push <filePath>

Options:
  -v, --verbose  show detail log
  -h, --help     Displays this help.
  --force        force push true or false

Arguments:
  push           push build result to repo
  filePath       bundle file path
```

The `ll-builder push` command reads the content of the `bundle` format package according to the file path, and transfers the software data and `bundle` format package to the server.

```bash
ll-builder push <org.deepin.demo-1.0.0_x86_64.uab>
```
