<!--
SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# 构建第一个如意玲珑应用

本章是开发者入门路线，将以 [deepin-calculator](https://github.com/linuxdeepin/deepin-calculator.git) 为例完成配置、构建、调试、运行验证和分发。如果您还没有安装或运行过如意玲珑应用，请先完成[快速上手](./quick-start.md)。

首先确认命令是否可用：

```bash
ll-builder --version
```

如果命令找不到，推荐使用安装脚本的 `full` 模式，同时安装如意玲珑和 `ll-builder` 构建工具：

```bash
curl -fsSL https://raw.githubusercontent.com/OpenAtom-Linyaps/linyaps/master/install.sh | sh -s -- full
```

如果您的发行版不受安装脚本支持，请按照[安装如意玲珑](./install.md)中的手动步骤安装 `ll-builder`。

## 配置项目

如意玲珑应用的应用 ID 使用倒置域名格式，至少包含一个 `.`，除了最后一节允许出现额外的 `-` 字符，前面的节只能包含数字、字母和 `_`。使用 ID `org.deepin.calculator` 创建项目：

```bash
ll-builder create org.deepin.calculator
```

该命令会创建项目目录，并在其中生成 `linglong.yaml` 模板：

```text
org.deepin.calculator/
└── linglong.yaml
```

进入项目目录：

```bash
cd org.deepin.calculator
```

打开生成的 `linglong.yaml`。模板已经根据创建项目时输入的应用 ID 填写了 `package.id`，其他字段仍是占位内容。接下来逐步替换这些占位内容，每完成一步都保存文件。

### 第一步：填写应用元数据

将模板中的 `package` 修改为：

```yaml
package:
  id: org.deepin.calculator
  name: deepin-calculator
  version: 5.7.21.0
  kind: app
  description: |
    calculator for deepin os.
```

这一步用于确定软件包的身份和基本信息：

- `id` 是仓库、安装和运行应用时使用的唯一标识。`ll-builder create` 已根据项目名称生成该值，不应随意修改。
- `name` 和 `description` 用于向用户说明应用名称与用途。
- `version` 是应用版本，此字段会参与版本号比较，以确定应用是否有更新。
- `kind: app` 表示该软件包是可以启动的应用，而不是运行时组件 Base、Runtime 或 Extension。

### 第二步：选择 Base 和 Runtime

将模板中的 `base` 修改为以下内容，并增加 `runtime`：

```yaml
base: org.deepin.base/23.1.0
runtime: org.deepin.runtime.dtk/23.1.0
```

Base 提供 glibc 等基础系统组件，Runtime 提供应用共享的框架库。本示例使用的 deepin-calculator 版本依赖 Qt5 和 DTK5，因此选择相互兼容的 `org.deepin.base/23.1.0` 与 `org.deepin.runtime.dtk/23.1.0`。

新应用选择环境时原则上应当选择最新的 Base 和 Runtime，必要时请查看[运行时组件](../reference/runtime.md)，选择仍处于维护状态的版本。

### 第三步：声明应用源码

取消模板中 `sources` 示例的注释，首先声明计算器源码：

```yaml
sources:
  - kind: git
    url: https://github.com/linuxdeepin/deepin-calculator.git
    version: master
    commit: d7e207b4a71bbd97f7d818de5044228c1a6e2c92
```

`sources` 告诉 `ll-builder` 在构建前获取哪些内容。使用完整的 `commit` 固定源码版本，可以避免上游分支变化导致同一份配置在不同时间产生不同结果。该源码会下载到 `/project/linglong/sources/deepin-calculator.git`。

deepin-calculator 还需要 `dde-qt-dbus-factory` 提供的 D-Bus 接口代码，这种 Base 和 Runtime 中没有提供的库，需要自行编译，并随应用分发。在 `sources` 列表末尾增加第二项，更新后的内容为：

```yaml
sources:
  - kind: git
    url: https://github.com/linuxdeepin/deepin-calculator.git
    version: master
    commit: d7e207b4a71bbd97f7d818de5044228c1a6e2c92
  - kind: git
    url: https://github.com/linuxdeepin/dde-qt-dbus-factory.git
    version: master
    commit: d952e1913172c5507af080f644a654f9ba5fed95
```

新的源码会下载到 `/project/linglong/sources/dde-qt-dbus-factory.git`。

### 第四步：编写构建脚本

模板中的 `build` 只会输出 `hello`，不会生成可运行的应用。将其替换为：

```yaml
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

先使用 qmake 构建并安装 `dde-qt-dbus-factory`，再使用 CMake 构建计算器。`${PREFIX}` 是如意玲珑为当前应用提供的安装前缀，`${TRIPLET}` 表示当前目标架构的库目录。所有最终产物都应安装到 `${PREFIX}`，不能直接写入构建容器的 `/usr`，当前的实现 `${PREFIX}` 位于 `/opt/apps/<appid>/files`。

### 第五步：设置启动命令

构建脚本已经确定可执行文件的安装位置。现在将模板中的演示命令替换为计算器的实际入口：

```yaml
command:
  - /opt/apps/org.deepin.calculator/files/bin/deepin-calculator
```

`command` 决定执行 `ll-builder run` 或 `ll-cli run org.deepin.calculator` 时启动哪个程序。把它放在构建脚本之后配置，可以根据实际安装结果填写路径，确保启动命令指向 `${PREFIX}/bin` 中生成的可执行文件。

### 第六步：核对完整配置

完成以上步骤后，`linglong.yaml` 应为：

```yaml
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

字段的完整说明见[构建配置文件简介](../building/manifests.md)。

## 构建

```bash
ll-builder build
```

等待如意玲珑应用构建完成。如果出现错误，请调整前面的构建配置后重试。

构建成功的应用会自动提交到本地的构建仓库，通过 `ll-builder list` 可以查看已经构建的程序，通过 `ll-builder remove` 可以从本地仓库移除提交的项目。构建通过且提交到本地仓库的应用可以直接运行验证或导出用来分发。

## 运行如意玲珑应用

验证应用运行使用：

```bash
ll-builder run
```

正常情况计算器启动会显示：

![org.deepin.calculator.png](./images/org.deepin.calculator.png)

如果需要调试应用，可以以调试模式进入应用运行的容器环境：

```bash
ll-builder run --debug -- bash
```

对于 GDB、gdbserver、Visual Studio Code 和 Qt Creator 的配置方法，参阅[调试如意玲珑应用](../debug/debug.md)。

## 导出和分发

本地验证通过后，可以导出 UAB 离线包：

```bash
ll-builder export --ref main:org.deepin.calculator/5.7.21.0/<arch>
```

其中 `<arch>` 是构建产物的目标架构，例如 `x86_64`、`arm64` 或 `loong64`。请通过 `ll-builder list` 查看构建结果，并将命令中的 ref 替换为列表显示的完整值。

在另一台已安装如意玲珑的机器上验证应用在真实用户环境下的运行：

```bash
ll-cli install ./org.deepin.calculator_5.7.21.0_<arch>_main.uab
ll-cli run org.deepin.calculator
```

UAB 文件名中的 `<arch>` 同样需要替换为实际目标架构；也可以直接使用导出命令生成的文件名。

需要发布到远程仓库时，配置仓库和凭据后执行 `ll-builder push`。具体参数和仓库说明见 [`ll-builder push`](../reference/commands/ll-builder/push.md)和[仓库管理](../publishing/repositories.md)。

## 继续阅读

- [ll-builder 工作流](./ll-builder-workflow.md)了解 ll-builder 工作原理
- [构建配置解读](../building/demo.md)以 linyaps 仓库的真实配置说明 Meson、Autotools 和 CMake 构建流程
- [从 deb 转换](../building/deb_conversion.md)以 Antigravity 为例子，演示如何从 deb 包转换
- [linglongdev](https://github.com/linglongdev) 维护的真实应用配置

## 视频链接

[同心联盟《开发赋能共建如意玲珑生态》分享直播会回看视频](https://www.bilibili.com/video/BV1ff421R7aY)
