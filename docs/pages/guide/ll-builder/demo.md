<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# demo示例

## 初始化如意玲珑工程

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
  id: org.deepin.runtime.dtk
  version: 23.1.0

source:
  kind: git
  url: "https://github.com/linuxdeepin/linglong-builder-demo.git"
  commit: 24f78c8463d87ba12b0ac393ec56218240315a9

build:
  kind: qmake
```

## 开始构建

在如意玲珑工程根目录下执行build子命令：

```bash
ll-builder build
```

## 运行应用

构建成功后，在如意玲珑工程目录下执行run子命令，可以直接运行应用而无需安装。

```bash
ll-builder run
```

## 查看构建内容

在如意玲珑工程根目录下执行export子命令，检出构建内容。

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

## 更多参考示例

[qt5](https://github.com/linglongdev/cn.org.linyaps.demo.qt5) - qt5程序

[dtk5](https://github.com/linglongdev/cn.org.linyaps.demo.dtk5.qmake) - dtk5 + qmake

[dtk5](https://github.com/linglongdev/cn.org.linyaps.demo.dtk5.cmake) - dtk5 + cmake

[dtkdeclarative5](https://github.com/linglongdev/cn.org.linyaps.demo.dtkdeclarative5) - dtk5 + qml

[electron](https://github.com/myml/electron-vue-linyaps-app) - electron + vue 例子

[plantuml](https://github.com/linglongdev/com.plantuml.gpl) - 一个java应用，使用编程的方式绘制流程图

[org.sumatrapdfreader](https://github.com/linglongdev/org.sumatrapdfreader) - 一个wine应用，pdf阅读器

## 更完整的示例

[完整示例](../start/how_to_use.md) - 包含了如何构建应用、导出构建内容、安装、运行等完整流程的示例。
