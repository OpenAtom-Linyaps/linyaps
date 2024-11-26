<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# 构建配置文件简介

`linglong.yaml` 是如意玲珑项目工程的描述文件，记录构建所需的相关信息。如构建产物的名称、版本、源码地址、构建依赖等。

## 工程目录结构

```bash
{project-root}
├── linglong.yaml
└── linglong

{user-home}
└── .cache
    └── linglong-builder
        ├── repo
        └── layers
```

## 字段定义

### 软件包元信息配置

```yaml
package:
  id: org.deepin.calculator
  name: deepin-calculator
  version: 5.7.21.0
  kind: app
  description: |
    calculator for deepin os.
```

| 名称        | 描述                                               |
| ----------- | -------------------------------------------------- |
| description | 构建产物的详细描述                                 |
| id          | 构建产物的唯一名称                                 |
| kind        | 构建产物的类型：app、runtime，依次代表应用、运行时 |
| version     | 构建产物的版本，要求四位数字。                     |

### 基础环境（base）

```bash
base: org.deepin.base/23.1.0
```

最小根文件系统。

| 名称    | 描述                                      |
| ------- | ----------------------------------------- |
| id      | base  的唯一名称                         |
| version | base 的版本号,  三位数可以模糊匹配第四位 |

### 运行时（runtime）

应用运行时依赖，同时也是构建依赖。

```text
runtime: org.deepin.runtime.dtk/23.1.0
```

| 名称    | 描述                                            |
| ------- | ----------------------------------------------- |
| id      | 运行时（runtime）的唯一名称                     |
| version | 运行时（runtime）版本，三位数可以模糊匹配第四位 |

### 源码

描述源码信息。

```yaml
sources:
  kind: git
  url: https://github.com/linuxdeepin/deepin-calculator.git
  version: master
  commit: d7e207b4a71bbd97f7d818de5044228c1a6e2c92
```

| 名称   | 描述                                                                  |
| ------ | --------------------------------------------------------------------- |
| kind   | 源码类型，可选类型 file、archive、git                                 |
| url    | 源码地址，类型为 file、archive、git 时填写                            |
| digest | 归档文件的 hash 值，使用 sha256 算法加密，类型为 file、archive 时填写 |
| commit | 源码某次提交 hash 值，类型为 git 时填写                               |

### 导出裁剪规则（UAB）

构建应用后导出到 UAB 包时，需要裁剪的文件如下例所示：

```yaml
exclude:
  - /usr/share/locale # 裁剪整个文件夹
  - /usr/lib/libavfs.a # 裁剪单个文件

include:
  - /usr/share/locale/zh_CN.UTF-8 # 配合exclude实现仅导出一个文件夹下的某些文件
```

| 名称    | 描述                                      |
| ------- | ----------------------------------------- |
| exclude | 容器内的绝对路径，可以为文件或文件夹      |
| include | 容器内的绝对路径，需要导入进 UAB 包的文件 |

### 构建规则

描述构建规则。

```yaml
build: |
  qmake -makefile PREFIX=${PREFIX} LIB_INSTALL_DIR=${PREFIX}/lib/${TRIPLET}
  make
  make install
```

| 名称  | 描述              |
| ----- | ----------------- |
| build | 构建时 build 规则 |

### 变量

描述构建可以使用的变量，配合 build 构建使用。

| 名称    | 描述                                                                                                   |
| ------- | ------------------------------------------------------------------------------------------------------ |
| PREFIX  | 环境变量之一，可在 variable、build 字段下使用；提供构建时的安装路径，如/opt/apps/org.deepin.calculator |
| TRIPLET | 环境变量之一，可在 variable、build 字段下使用；提供包含架构信息的三元组，如 x86_64-linux-gnu           |

## 完整示例

### 构建根文件系统

```bash
git clone git@github.com:linglongdev/org.deepin.foundation.git
cd org.deepin.foundation
bash build_base.sh beige amd64
```

该项目用来构建如意玲珑使用的根文件系统。eagle 指发行版代号，amd64 指架构。

| 发行版            | 架构                      |
| ----------------- | ------------------------- |
| eagle (UOS 20)    | amd64、arm64、loongarch64 |
| beige (deepin 23) | amd64、arm64              |

### 构建运行时

```yaml
git clone git@github.com:linglongdev/org.deepin.Runtime.git -b v23
cd org.deepin.Runtime
./depend-deb-list.sh | ./tools/download_deb_depend.bash
ll-builder build --skip-fetch-source
```

在根文件系统基础上添加 Qt 等基础环境。

### 构建应用

#### 计算器

```yaml
version: '1'

package:
  id: org.deepin.calculator
  name: deepin-calculator
  version: 5.7.21.0
  kind: app
  description: |
    calculator for deepin os.

command:
  - /opt/apps/org.deepin.calculator/files/bin/deepin-calculator

base: org.deepin.base/23.1.0
runtime: org.deepin.runtime.dtk/23.1.0

sources:
  - kind: git
    url: https://github.com/linuxdeepin/deepin-calculator.git
    version: master
    commit: d7e207b4a71bbd97f7d818de5044228c1a6e2c92

  - kind: git
    url: https://github.com/linuxdeepin/dde-qt-dbus-factory.git
    version: master
    commit: d952e1913172c5507af080f644a654f9ba5fed95

build: |
  # build dde-qt-dbus-factory
  cd /project/linglong/sources/dde-qt-dbus-factory.git
  qmake -makefile \
        PREFIX=${PREFIX} \
        LIB_INSTALL_DIR=${PREFIX}/lib/${TRIPLET} \
        INSTALL_ROOT=${PREFIX}

  make
  make install

  # build calculator
  cd /project/linglong/sources/deepin-calculator.git
  cmake -Bbuild \
        -DCMAKE_INSTALL_PREFIX=${PREFIX} \
        -DCMAKE_INSTALL_LIBDIR=${PREFIX}/lib/${TRIPLET} \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_SAFETYTEST_ARG="CMAKE_SAFETYTEST_ARG_OFF" \
        -DAPP_VERSION=5.7.21 \
        -DVERSION=5.7.21

  cmake --build build
  cmake --build build --target install
```
