<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# Export Layer File

Use `ll-builder export` to check out the build content and generate layer file.

View the help information for the `ll-builder export` command:

```bash
ll-builder export --help
```

Here is the output:

```text
Usage: ll-builder [options]

Options:
  -v, --verbose  show detail log (deprecated, use QT_LOGGING_RULES)
  -h, --help     Displays help on commandline options.
  --help-all     Displays help including Qt specific options.
```

The `ll-builder export` command creates a directory named `appid` in the project root directory, then checks out the local build cache to this directory, and generate layer file to the build result.

An example of the `ll-builder export` command is as follows:

```bash
ll-builder export
```

The directory structure after checkout is as follows:

```text
linglong.yaml org.deepin.demo_0.0.0.1_x86_64_develop.layer org.deepin.demo_0.0.0.1_x86_64_runtime.layer
```

Layer files are divided into two categories: `runtime` and `develop`. The `runtime` includes the application's execution environment, while the `develop` layer, built upon the `runtime`, retains the debugging environment.

Take the `org.deepin.demo` Linglong application as an example. The directory is as follows:

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
