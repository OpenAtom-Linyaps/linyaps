<!--
SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# 构建玲珑应用

这里以 [deepin-calculator](https://github.com/linuxdeepin/deepin-calculator.git) 作为从源码构建玲珑应用的例子。

玲珑应用的 appid 需要使用倒置域名的方式命名。

## 创建项目

```bash
mkdir org.deepin.calculator
```

目录下创建 linglong.yaml 文件

```bash
touch org.deepin.calculator/linglong.yaml
```

进入目录

```bash
cd org.deepin.calculator
```

使用文本编辑器编辑 linglong.yaml

```bash
version: "1"

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
        -DCMAKE_INSTALL_LIBDIR=${PREFIX}/lib/${TRIPLET}
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_SAFETYTEST_ARG="CMAKE_SAFETYTEST_ARG_OFF" \
        -DAPP_VERSION=5.7.21 \
        -DVERSION=5.7.21

  cmake --build build
  cmake --build build --target install
```

linglong.yaml 文件遵循 yaml 语法规范编写的。

linglong.yaml 中字段的详细解释参考：[构建配置文件简介](../ll-builder/manifests.md)

## 构建

```bash
ll-builder build
```

等待玲珑应用构建完成。

## 运行玲珑应用

```bash
ll-builder run
```

`ll-builder run` 运行成功输出如下：

![org.deepin.demo.png](../ll-builder/images/org.deepin.demo.png)

为了便于调试，使用额外的 `--exec /bin/bash`参数可替换进入容器后默认执行的程序，如：

```bash
ll-builder run --exec /bin/bash
```

<br>
<br>

# 转换 deb 应用

这里使用百度网盘作为 deb 转换玲珑包的例子。

## 获取软件包

以百度网盘为例。先获取 deb 包文件。目前只支持转换遵循应用商店打包规范的软件。

```bash
apt download com.baidu.baidunetdisk
```

## 转换应用

```bash
ll-pica convert -c com.baidu.baidunetdisk_4.17.7_amd64.deb -w work -b
```

进入目录

```bash
cd work/package/com.baidu.baidunetdisk/amd64
```

## 安装 layer 文件

```bash
ll-cli install ./com.baidu.baidunetdisk_4.17.7.0_x86_64_runtime.layer
```

## 运行应用

```
ll-cli run com.baidu.baidunetdisk
```

运行成功输出如下：

![img](images/com.baidu.baidunetdisk.png)
