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

`linglong.yaml` 文件结构遵循特定的规范。首先，需要在文件顶层声明配置文件的版本：

```yaml
version: "1"
```

| 名称    | 描述                           | 必填 |
| ------- | ------------------------------ | ---- |
| version | 构建配置文件的版本，当前为 '1' | 是   |

接下来是主要的配置块。其中 `package`, `base`, `build` 是必须定义的。

### 软件包元信息配置 (`package`)

定义了构建产物的基本信息。此 `package` 配置块是必需的。

```yaml
package:
  id: org.deepin.calculator
  name: deepin-calculator
  version: 5.7.21.0
  kind: app
  description: |
    calculator for deepin os.
  architecture: amd64 # 可选
  channel: stable # 可选
```

| 名称         | 描述                                               | 必填 |
| ------------ | -------------------------------------------------- | ---- |
| id           | 构建产物的唯一名称 (例如：`org.deepin.calculator`) | 是   |
| name         | 构建产物的名称 (例如：`deepin-calculator`)         | 是   |
| version      | 构建产物的版本，建议四位数字 (例如：`5.7.21.0`)    | 是   |
| kind         | 构建产物的类型：`app` (应用)、`runtime` (运行时)   | 是   |
| description  | 构建产物的详细描述                                 | 是   |
| architecture | 构建产物的目标架构 (例如：`amd64`, `arm64`)        | 否   |
| channel      | 构建产物的通道 (例如：`stable`, `beta`)            | 否   |

### 启动命令 (`command`)

定义如何启动应用。这是一个字符串列表，列表中的第一个元素通常是可执行文件的绝对路径（相对于容器内部），后续元素是传递给可执行文件的参数。

```yaml
command:
  - /opt/apps/org.deepin.calculator/files/bin/deepin-calculator
  # - --some-argument # 可以添加更多参数
```

| 名称    | 描述                                                                  | 必填 |
| ------- | --------------------------------------------------------------------- | ---- |
| command | 定义启动应用的可执行文件路径及其参数列表。对于 `kind: app` 通常需要。 | 否   |

### 基础环境 (`base`)

指定构建和运行所需的最小根文件系统。此字段是必需的。

```bash
base: org.deepin.base/23.1.0
```

| 名称 | 描述                                                         | 必填 |
| ---- | ------------------------------------------------------------ | ---- |
| base | base 的标识符，格式为 `id/version`。版本号支持三位模糊匹配。 | 是   |

### 运行时（runtime）

应用运行时依赖，同时也是构建依赖。

```text
runtime: org.deepin.runtime.dtk/23.1.0
```

| 名称    | 描述                                            |
| ------- | ----------------------------------------------- |
| id      | 运行时（runtime）的唯一名称                     |
| version | 运行时（runtime）版本，三位数可以模糊匹配第四位 |

### 源码 (`sources`)

描述项目所需的源码信息。`sources` 是一个列表，可以包含多个源码项。获取后的源码默认存放在 `linglong.yaml` 同级路径的 `linglong/sources` 目录下。

#### git 类型

```yaml
sources:
  - kind: git
    url: https://github.com/linuxdeepin/deepin-calculator.git
    commit: d7e207b4a71bbd97f7d818de5044228c1a6e2c92 # 分支、tag或commit，用于精确指定提交
    name: deepin-calculator.git # 可选，指定下载后的目录名
```

| 名称    | 描述                                                         | 必填 (单个source内) |
| ------- | ------------------------------------------------------------ | ------------------- |
| kind    | `git`，表示使用 git 工具下载。                               | 是                  |
| url     | 源码仓库地址                                                 | 是                  |
| commit  | 源码仓库分支、标签或者某次提交的hash值，用于精确检出                         | 是                  |
| name    | 可选，指定源码下载后在 `linglong/sources` 目录下的子目录名。 | 否                  |

#### file 类型

```yaml
sources:
  - kind: file
    url: https://example.com/some-file.dat
    digest: sha256:... # 建议提供 sha256 哈希值
    name: my-data.dat # 可选，指定下载后的文件名
```

| 名称   | 描述                                                   | 必填 (单个source内) |
| ------ | ------------------------------------------------------ | ------------------- |
| kind   | `file`，表示直接下载文件。                             | 是                  |
| url    | 文件下载地址                                           | 是                  |
| digest | 可选，文件的 sha256 哈希值，用于校验。                 | 否                  |
| name   | 可选，指定下载后在 `linglong/sources` 目录下的文件名。 | 否                  |

#### archive 类型

```yaml
sources:
  - kind: archive
    url: https://github.com/linuxdeepin/deepin-calculator/archive/refs/tags/6.5.4.tar.gz
    digest: 9675e27395891da9d9ee0a6094841410e344027fd81265ab75f83704174bb3a8 # 建议提供 sha256 哈希值
    name: deepin-calculator-6.5.4 # 可选，指定解压后的目录名
```

| 名称   | 描述                                                   | 必填 (单个source内) |
| ------ | ------------------------------------------------------ | ------------------- |
| kind   | `archive`，下载压缩包并自动解压。支持常见的压缩格式。  | 是                  |
| url    | 压缩包下载地址                                         | 是                  |
| digest | 可选，压缩包文件的 sha256 哈希值，用于校验。           | 否                  |
| name   | 可选，指定解压后在 `linglong/sources` 目录下的目录名。 | 否                  |

#### dsc 类型

```yaml
sources:
  - kind: dsc
    url: https://cdn-community-packages.deepin.com/deepin/beige/pool/main/d/deepin-calculator/deepin-calculator_6.0.1.dsc
    digest: ce47ed04a427a887a52e3cc098534bba53188ee0f38f59713f4f176374ea2141 # 建议提供 sha256 哈希值
    name: deepin-calculator-dsc # 可选，指定下载和解压后的目录名
```

| 名称   | 描述                                                         | 必填 (单个source内) |
| ------ | ------------------------------------------------------------ | ------------------- |
| kind   | `dsc`，处理 Debian 源码包描述文件及其关联文件。              | 是                  |
| url    | `.dsc` 文件下载地址                                          | 是                  |
| digest | 可选，`.dsc` 文件的 sha256 哈希值，用于校验。                | 否                  |
| name   | 可选，指定下载和解压后在 `linglong/sources` 目录下的目录名。 | 否                  |

### 导出裁剪规则 (`exclude`/`include`)

构建应用后导出到 UAB 包时，需要裁剪的文件如下例所示：

```yaml
exclude:
  - /usr/share/locale # 裁剪整个文件夹
  - /usr/lib/libavfs.a # 裁剪单个文件

include:
  - /usr/share/locale/zh_CN.UTF-8 # 配合exclude实现仅导出一个文件夹下的某些文件
```

| 名称    | 描述                                                                |
| ------- | ------------------------------------------------------------------- |
| exclude | 容器内的绝对路径，可以为文件或文件夹，用于排除。                    |
| include | 容器内的绝对路径，需要强制包含进 UAB 包的文件（即使父目录被排除）。 |

### 构建规则 (`build`)

描述如何在构建环境中编译和安装项目。此字段是必需的，其内容为一个多行字符串，将被当作 shell 脚本执行。

描述构建规则。

```yaml
build: |
  qmake -makefile PREFIX=${PREFIX} LIB_INSTALL_DIR=${PREFIX}/lib/${TRIPLET}
  make
  make install
```

| 名称  | 描述                                                  | 必填 |
| ----- | ----------------------------------------------------- | ---- |
| build | 构建阶段执行的 shell 脚本。脚本在容器构建环境内执行。 | 是   |

### 构建扩展 (`buildext`)

可选字段，用于定义特定构建阶段的扩展行为。目前主要支持 `apt` 扩展，用于管理构建和运行时所需的 Debian 包依赖。

```yaml
buildext:
  apt:
    build_depends: # 构建时依赖，仅在构建环境中安装
      - build-essential
      - cmake
      - qt6-base-dev
    depends: # 运行时依赖，会包含在最终的应用或运行时中
      - libqt6widgets6
      - libglib2.0-0
```

| 名称          | 描述                                                               | 必填 |
| ------------- | ------------------------------------------------------------------ | ---- |
| buildext      | 构建扩展配置的容器块。                                             | 否   |
| apt           | 使用 apt 包管理器的扩展配置。                                      | 否   |
| build_depends | 一个字符串列表，列出了构建时需要的包，这些包不会进入最终产物。     | 否   |
| depends       | 一个字符串列表，列出了运行时需要的包，这些包会被包含在最终产物中。 | 否   |

### 模块 (`modules`)

可选字段，用于将安装到 `${PREFIX}` 目录下的文件拆分成不同的模块。这对于按需下载或提供可选功能很有用。

```yaml
modules:
  - name: main # 主模块名，通常与 package.id 相同或相关
    files: # 此模块包含的文件或目录列表 (相对于 ${PREFIX})
      - bin/
      - share/applications/
      - share/icons/
      - lib/ # 包含所有库
  - name: translations # 翻译模块
    files:
      - share/locale/
  - name: extra-data # 可选数据模块
    files:
      - share/my-app/optional-data/
```

| 名称    | 描述                                                                            | 必填                |
| ------- | ------------------------------------------------------------------------------- | ------------------- |
| modules | 定义应用模块拆分的规则列表。                                                    | 否                  |
| name    | 模块的名称。每个模块都需要一个唯一的名称。                                      | 是 (在每个模块项内) |
| files   | 一个字符串列表，列出了属于该模块的文件或目录。路径是相对于 `${PREFIX}` 的路径。 | 是 (在每个模块项内) |

**注意:** 所有安装到 `${PREFIX}` 下的文件都会被分配到一个模块中。未定义 `modules` 时，构建系统会自动生成默认的 `binary` 和 `develop` 模块。

### 变量

描述构建过程走可以使用的变量。

| 名称    | 描述                                                                                 |
| ------- | ------------------------------------------------------------------------------------ |
| PREFIX  | build 字段下使用的环境变量；提供构建时的安装路径，如 /opt/apps/org.deepin.calculator |
| TRIPLET | build 字段下使用的环境变量；提供包含架构信息的三元组，如 x86_64-linux-gnu            |

## 完整示例

### 构建应用

#### 计算器

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

buildext:
  apt:
    # 构建时额外依赖的包
    build_depends: []
    # 运行时额外依赖的包
    depends: []
```

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
