<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# 导出uab格式应用

`ll-builder export`命令用来检出构建内容, 生成`bundle`格式软件包（简称 uab）。

查看`ll-builder export`命令的帮助信息：

```bash
ll-builder export --help
```

`ll-builder export`命令的帮助信息如下：

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

`ll-builder export`命令在工程根目录下创建以`appid`为名称的目录，并将本地构建缓存检出到该目录。同时根据该构建结果制作出`bundle`格式软件包。

`ll-builder export`命令使用示例如下：

```bash
ll-builder export
```

检出后的目录结构如下：

```text
org.deepin.demo linglong.yaml org.deepin.demo_0.0.1_x86_64.uab
```

以`org.deepin.demo`玲珑应用为例，目录如下：

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
