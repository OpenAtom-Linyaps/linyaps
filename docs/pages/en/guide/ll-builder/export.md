<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# Export layer file or uab file

Use `ll-builder export` to check out the build content and generate layer file or uab file.

View the help information for the `ll-builder export` command:

```bash
ll-builder export --help
```

Here is the output:

```text
Export to linyaps layer or uab
Usage: ll-builder export [OPTIONS]

Options:
  -h,--help                   Print this help message and exit
  --help-all                  Expand all help
  --file FILE:FILE [./linglong.yaml]
                              File path of the linglong.yaml
  --icon FILE:FILE            Uab icon (optional)
  --layer                     Export to linyaps layer file
```

The `ll-builder export` command creates a directory named `appid` in the project root directory, then checks out the local build cache to this directory, and generate layer file to the build result.

An example of the `ll-builder export` command is as follows:

## Export layer file

```bash
ll-builder export --layer
```

`Tips: When the linyaps version is greater than 1.5.6, export defaults to exporting the uab file. If you want to export a layer file, you need to add the --layer parameter`

The directory structure after checkout is as follows:

```text
linglong.yaml demo_0.0.0.1_x86_64_develop.layer demo_0.0.0.1_x86_64_binary.layer
```

Layer files are divided into two categories: `binary` and `develop`. The `binary` includes the application's execution environment, while the `develop` layer, built upon the `binary`, retains the debugging environment.

## Export Uab File

```bash
ll-builder export
```

The uab file is an offline distribution format used by the linyaps software package, which is not suitable for systems that can normally connect to the linyaps repository. Instead, one should utilize the delta transfer scheme provided by the linyaps software repository to reduce the network transmission size.

Take the `org.deepin.demo` linyaps application as an example. The directory is as follows:

```text
org.deepin.demo
├── entries
│   └── share -> ../files/share
├── files
│   ├── bin
│   │   └── demo
│   └── share
│       ├── applications
│       │   └── demo.desktop
│       └── systemd
│           └── user -> ../../lib/systemd/user
├── info.json
└── org.deepin.demo.install
```
