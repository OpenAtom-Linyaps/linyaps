<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# Export Uab Format

Use `ll-builder export` to check out the build content and generate the `bundle` format package (referred to as uab).

View the help information for the `ll-builder export` command:

```bash
ll-builder export --help
```

Here is the output:

```text
Usage: ll-builder [options] export [filename]

Options:
  -v, --verbose  show detail log
  -h, --help     Displays this help.

Arguments:
  export         export build result to uab bundle
  filename       bundle file name , if filename is empty,export default format
                 bundle
```

The `ll-builder export` command creates a directory named `appid` in the project root directory, then checks out the local build cache to this directory, and produces a `bundle` format package according to the build result.

An example of the `ll-builder export` command is as follows:

```bash
ll-builder export
```

The directory structure after checkout is as follows:

```text
org.deepin.demo linglong.yaml org.deepin.demo_0.0.1_x86_64.uab
```

Linglong applications have two package formats: `linglong` and `uab`. Currently the `linglong` package format is the main one.

Take the `org.deepin.demo` Linglong application as an example. The directory is as follows:

```text
org.deepin.demo
├── entries
│   └── applications
│        └── demo.desktop
├── files
│   └── bin
│       └── demo
├── info.json
└── linglong.yaml
```
