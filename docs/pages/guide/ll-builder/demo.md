<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# demo示例

## 初始化玲珑工程

```bash
ll-builder create org.deepin.demo
```

## 编辑linglong.yaml

### 填写软件包元信息

```yaml
package:
  id: org.deepin.demo
  name: deepin-demo
  version: 0.0.1
  kind: app
  description: |
    simple Qt demo.
```

### 填写运行时信息

```yaml
runtime:
  id: org.deepin.Runtime
  version: 23.0.0
```

### 填写源码信息

使用git源码

```yaml
source:
  kind: git
  url: "https://github.com/linuxdeepin/linglong-builder-demo.git"
  commit: 24f78c8463d87ba12b0ac393ec56218240315a9
```

### 选择构建模板

源码为qmake工程，填写build 类型为qmake（模板内容见qmake.yaml）。

```yaml
build:
  kind: qmake
```

### 完整linglong.yaml

```yaml
package:
  id: org.deepin.demo
  name: deepin-demo
  version: 0.0.1
  kind: app
  description: |
    simple Qt demo.

runtime:
  id: org.deepin.Runtime
  version: 23.0.0

source:
  kind: git
  url: "https://github.com/linuxdeepin/linglong-builder-demo.git"
  commit: 24f78c8463d87ba12b0ac393ec56218240315a9

build:
  kind: qmake
```

## 开始构建

在玲珑工程根目录下执行build子命令：

```bash
ll-builder build
```

## 运行应用

构建成功后，在玲珑工程目录下执行run子命令，可以直接运行应用而无需安装。

```bash
ll-builder run
```

## 查看构建内容

在玲珑工程根目录下执行export子命令，检出构建内容。

```bash
ll-builder export --layer
```

目录结构如下：

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
