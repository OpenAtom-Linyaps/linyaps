<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# 导出 layer 文件 或 uab 文件

`ll-builder export`命令用来检出构建内容, 生成 layer 或 uab 文件。

查看 `ll-builder export`命令的帮助信息：

```bash
ll-builder export --help
```

`ll-builder export`命令的帮助信息如下：

```text
导出如意玲珑layer或uab
用法: ll-builder export [选项]

Options:
  -h,--help                   打印帮助信息并退出
  --help-all                  展开所有帮助
  --file FILE:FILE [./linglong.yaml]
                              linglong.yaml的文件路径
  --icon FILE:FILE            Uab图标（可选）
  --layer                     导出如意玲珑layer文件
```

`ll-builder export`命令在工程根目录下创建以 `appid` 为名称的目录，并将本地构建缓存检出到该目录。同时根据该构建结果生成 layer 文件。

`ll-builder export`命令使用示例如下：

## 导出 layer 文件

```bash
ll-builder export --layer
```

`Tips: 在如意玲珑版本大于1.5.6时，export 默认导出 uab 包，如果要导出 layer 文件，需要加上 --layer 参数`

检出后的目录结构如下：

```text
linglong.yaml demo_0.0.0.1_x86_64_develop.layer demo_0.0.0.1_x86_64_binary.layer
```

layer 文件分为，binary 和 develop, binary 包含应用的运行环境，develop 包含应用的调试环境。

## 导出 uab 文件

```bash
ll-builder export
```

uab 文件，是如意玲珑软件包使用的离线分发格式，并不适合可以正常连接到如意玲珑仓库的系统使用，应当使用如意玲珑软件仓库提供的增量传输方案以减少网络传输体积。

以 `org.deepin.demo` 如意玲珑应用为例，目录如下：

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
