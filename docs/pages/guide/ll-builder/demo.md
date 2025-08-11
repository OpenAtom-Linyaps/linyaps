<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# 构建示例演示

## 初始化玲珑应用项目

```bash
ll-builder create org.deepin.demo
```

## 编辑 linglong.yaml 配置文件

### 配置软件包元数据信息

```yaml
package:
  id: org.deepin.demo
  name: demo
  kind: app
  version: 1.0.0.0
  description: |
    A simple demo app.
```

### 配置应用程序启动命令

```yaml
command:
  - demo
```

### 配置基础系统和运行时环境

```yaml
base: org.deepin.base/23.1.0
runtime: org.deepin.runtime.dtk/23.1.0
```

### 配置源代码信息

使用 Git 拉取源码

```yaml
sources:
  - kind: git
    url: "https://github.com/linuxdeepin/linglong-builder-demo.git"
    commit: master
    name: linglong-builder-demo
```

### 配置构建规则

```yaml
build: |
  cd /project/linglong/sources/linglong-builder-demo
  rm -rf build || true
  mkdir build
  cd build
  qmake ..
  make
  make install
```

### 完整的 linglong.yaml 配置文件

```yaml
version: "1"

package:
  id: org.deepin.demo
  name: demo
  kind: app
  version: 1.0.0.0
  description: |
    A simple demo app.

command:
  - demo

base: org.deepin.base/23.1.0
runtime: org.deepin.runtime.dtk/23.1.0

sources:
  - kind: git
    url: "https://github.com/linuxdeepin/linglong-builder-demo.git"
    commit: master
    name: linglong-builder-demo

build: |
  cd /project/linglong/sources/linglong-builder-demo
  rm -rf build || true
  mkdir build
  cd build
  qmake ..
  make
  make install
```

更多配置文件字段定义请参考[配置文件说明](./manifests.md)

## 执行构建流程

在玲珑项目根目录下执行构建命令：

```bash
ll-builder build
```

## 运行应用程序

构建成功后，在玲珑项目目录下执行运行命令，无需安装即可直接运行应用程序。

```bash
ll-builder run
```

## 导出构建产物

在玲珑项目根目录下执行导出命令，检出构建内容。

```bash
ll-builder export --layer
```

导出后的目录结构如下：

```text
├── linglong
├── linglong.yaml
├── org.deepin.demo_1.0.0.0_x86_64_binary.layer
└── org.deepin.demo_1.0.0.0_x86_64_develop.layer
```

## 更多参考示例

[qt5](https://github.com/linglongdev/cn.org.linyaps.demo.qt5) - qt5 程序

[dtk5](https://github.com/linglongdev/cn.org.linyaps.demo.dtk5.qmake) - dtk5 + qmake

[dtk5](https://github.com/linglongdev/cn.org.linyaps.demo.dtk5.cmake) - dtk5 + cmake

[dtkdeclarative5](https://github.com/linglongdev/cn.org.linyaps.demo.dtkdeclarative5) - dtk5 + qml

[electron](https://github.com/myml/electron-vue-linyaps-app) - electron + vue 例子

[plantuml](https://github.com/linglongdev/com.plantuml.gpl) - 一个 java 应用，使用编程的方式绘制流程图

[org.sumatrapdfreader](https://github.com/linglongdev/org.sumatrapdfreader) - 一个 wine 应用，pdf 阅读器

## 更完整的示例

[完整示例](../start/how_to_use.md) - 包含了如何构建应用、导出构建内容、安装、运行等完整流程的示例。
