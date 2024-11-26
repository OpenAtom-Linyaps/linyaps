<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# Push UAB to Remote Repositories

Use the `ll-builder push` command to push linyaps packages to linyaps remote repositories.

View the help information for the `ll-builder push` command:

```bash
ll-builder push --help
```

Here is the output:

```text
Push linyaps app to remote repo
Usage: ll-builder push [OPTIONS]

Options:
  -h,--help                   Print this help message and exit
  --help-all                  Expand all help
  --file FILE:FILE [./linglong.yaml]
                              File path of the linglong.yaml
  --repo-url URL              Remote repo url
  --repo-name NAME            Remote repo name
  --module TEXT               Push single module
```

The `ll-builder push` command reads the content of the `bundle` format package according to the file path, and transfers the software data and `bundle` format package to the server.

```bash
ll-builder push <org.deepin.demo-1.0.0_x86_64.uab>
```
