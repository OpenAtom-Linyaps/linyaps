<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# 导出 layer 文件

`ll-builder export`命令用来检出构建内容, 生成 layer 文件。

查看 `ll-builder export`命令的帮助信息：

```bash
ll-builder export --help
```

`ll-builder export`命令的帮助信息如下：

```text
Usage: ll-builder [options]

Options:
  -v, --verbose  show detail log (deprecated, use QT_LOGGING_RULES)
  -h, --help     Displays help on commandline options.
  --help-all     Displays help including Qt specific options.
```

`ll-builder export`命令在工程根目录下创建以 `appid`为名称的目录，并将本地构建缓存检出到该目录。同时根据该构建结果生成 layer 文件。

`ll-builder export`命令使用示例如下：

```bash
ll-builder export
```

检出后的目录结构如下：

```text
linglong.yaml org.deepin.demo_0.0.0.1_x86_64_develop.layer org.deepin.demo_0.0.0.1_x86_64_runtime.layer
```

layer 文件分为，runtime 和 develop, runtime 包含应用的运行环境，develop 在 runtime 的基础上保留调试环境。

以 `org.deepin.demo` 玲珑应用为例，目录如下：

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
