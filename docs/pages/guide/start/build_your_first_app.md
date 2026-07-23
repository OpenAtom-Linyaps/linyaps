<!--
SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# 构建第一个如意玲珑应用

本章是开发者入门路线，将以 [deepin-calculator](https://github.com/linuxdeepin/deepin-calculator.git) 为例完成配置、构建、调试、运行验证和分发。如果您还没有安装或运行过如意玲珑应用，请先完成[快速上手](./quick-start.md)。

开始前，请按照[安装如意玲珑](./install.md)安装 `ll-builder`，并确认命令可用：

```bash
ll-builder --version
```

## ll-builder 工作流

| 阶段 | 主要工作 | 命令或参考 |
| --- | --- | --- |
| 编写配置 | 定义应用元数据、启动命令、Base、Runtime、源码和构建脚本 | [构建配置文件简介](../building/manifests.md) |
| 构建 | 拉取依赖和源码，在隔离环境中编译并提交产物 | [`ll-builder build`](../reference/commands/ll-builder/build.md) |
| 调试 | 进入包含 develop 模块的容器，使用调试器定位问题 | [调试如意玲珑应用](../debug/debug.md) |
| 运行验证 | 不安装应用，直接运行本地构建结果 | [`ll-builder run`](../reference/commands/ll-builder/run.md) |
| 分发 | 导出 UAB，或将构建结果推送到远程仓库 | [`ll-builder export`](../reference/commands/ll-builder/export.md)、[`ll-builder push`](../reference/commands/ll-builder/push.md) |

完整命令列表见 [`ll-builder` 命令参考](../reference/commands/ll-builder/ll-builder.md)。

## 从源码构建

如意玲珑应用的应用 ID 使用倒置域名格式。一个最基本的 `linglong.yaml` 包含应用元数据、启动命令、Base、可选的 Runtime、源码和构建脚本。

## 创建项目

使用应用 ID 创建项目：

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
- `version` 对应本次打包应用版本，此字段会参与版本号比较，以确定应用是否有更新。
- `kind: app` 表示该软件包是可以启动的应用，而不是 Base、Runtime 或 Extension。

模板最上方的 `version: "1"` 是构建配置文件的格式版本，应保留，不要与 `package.version` 混淆。

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

`sources` 告诉 `ll-builder` 在构建前获取哪些内容。使用完整的 `commit` 固定源码版本，可以避免上游分支变化导致同一份配置在不同时间产生不同结果。该源码会放到 `/project/linglong/sources/deepin-calculator.git`。

### 第四步：添加构建依赖源码

deepin-calculator 还需要 `dde-qt-dbus-factory` 提供的 D-Bus 接口代码，这种不在 Base 和 Runtime 中提供的库，需要自行编译提供。在 `sources` 列表末尾增加第二项，更新后的内容为：

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

现在两个源码会分别下载到 `/project/linglong/sources/deepin-calculator.git` 和 `/project/linglong/sources/dde-qt-dbus-factory.git`。将应用及其构建依赖分别声明，可以明确源码来源，并让 `ll-builder` 在进入构建阶段前准备好全部内容，保证每次构建结果是可靠/可追溯的。

### 第五步：编写构建脚本

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

构建顺序与依赖关系一致：先使用 qmake 构建并安装 `dde-qt-dbus-factory`，再使用 CMake 构建计算器。`${PREFIX}` 是如意玲珑为当前应用提供的安装前缀，`${TRIPLET}` 表示当前目标架构的库目录。所有最终产物都应安装到 `${PREFIX}`，不能直接写入构建容器的 `/usr`，当前的实现 `${PREFIX}` 位于 `/opt/apps/<appid>/files`。

### 第六步：设置启动命令

构建脚本已经确定可执行文件的安装位置。现在将模板中的演示命令替换为计算器的实际入口：

```yaml
command:
  - /opt/apps/org.deepin.calculator/files/bin/deepin-calculator
```

`command` 决定执行 `ll-builder run` 或 `ll-cli run org.deepin.calculator` 时启动哪个程序。把它放在构建脚本之后配置，可以根据实际安装结果填写路径，确保启动命令指向 `${PREFIX}/bin` 中生成的可执行文件。路径不一致时，应用可能构建成功但无法启动。

### 第七步：核对完整配置

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

确认模板中的 `your name`、`your description`、`echo` 等占位内容已经全部删除。字段的完整说明见[构建配置文件简介](../building/manifests.md)。

## 构建

```bash
ll-builder build
```

等待如意玲珑应用构建完成。

构建完成的应用会自动提交到本地的构建仓库，通过 `ll-builder list` 可以查看已经构建的程序，通过 `ll-builder remove` 从本地仓库移除提交的项目。构建通过且提交到本地仓库的应用可以直接运行验证或导出用来分发。

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

## 更多构建示例

- [构建配置示例解读](../building/demo.md)以 Linyaps 仓库的真实配置说明 Meson、Autotools 和 CMake 构建流程
- [手动从 deb 转换](../building/deb_conversion.md)
- [使用 ll-pica 转换 deb 应用](../building/using_ll-pica.md)
- [linglongdev](https://github.com/linglongdev) 维护的真实应用配置


## 理解工作原理

完成第一个应用后，可以阅读[基础概念介绍](../reference/basic-concepts.md)，了解如意玲珑包类型、Base、Runtime、Extension、容器、OSTree，以及 `ll-cli`、`ll-builder`、包管理服务和转换工具之间的关系。

## 视频链接

[同心联盟《开发赋能共建如意玲珑生态》分享直播会回看视频](https://www.bilibili.com/video/BV1ff421R7aY)
