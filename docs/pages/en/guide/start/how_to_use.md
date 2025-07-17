<!--
SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# Build linyaps applications

Taking [deepin-calculator](https://github.com/linuxdeepin/deepin-calculator.git) as an example, introduce the process of building a linyaps package from source code.

## Create linyaps project

```bash
mkdir org.deepin.calculator
```

Create a file named `linglong.yaml` in the directory.

```bash
touch org.deepin.calculator/linglong.yaml
```

Enter the directory

```bash
cd org.deepin.calculator
```

Edit the linglong.yaml file using a text editor.

```bash
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

base: org.deepin.foundation/23.0.0
runtime: org.deepin.Runtime/23.0.1

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

The file "linglong.yaml" is written following the YAML syntax specifications.

Detailed explanation of fields in linglong.yaml for reference: [Manifests](../ll-builder/manifests.md)

## Build

```bash
ll-builder build
```

## Run

```bash
ll-builder run
```

the successful output of `ll-builder run` is as follows:

![org.deepin.calculator.png](./images/org.deepin.calculator.png)

For debugging purposes, use the additional `--exec /bin/bash` parameter to replace the default program executed upon entering the container, for example:

```bash
ll-builder run --exec /bin/bash
```

# Conversion application

Here, we use baidunetdisk as an example. We will introduce the process of converting DEB packages into linyaps packages

## Obtain software package

First, obtain the deb package file.
Currently, only software following the application store packaging
specifications is supported for conversion.

```bash
apt download com.baidu.baidunetdisk
```

## Conversion

```bash
ll-pica convert -c com.baidu.baidunetdisk_4.17.7_amd64.deb -w work -b --exportFile layer
```

Enter the directory

```bash
cd work/package/com.baidu.baidunetdisk/amd64
```

Installed using the `ll-cli install` command.

```bash
ll-cli install ./com.baidu.baidunetdisk_4.17.7.0_x86_64_runtime.layer
```

Successful execution output as follows:

![img](images/com.baidu.baidunetdisk.png)
