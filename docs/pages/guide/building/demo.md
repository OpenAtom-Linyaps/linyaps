<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# 构建配置示例解读

本章以 Linyaps 仓库 `master` 分支中的 [`linglong.yaml`](https://github.com/OpenAtom-Linyaps/linyaps/blob/master/linglong.yaml) 为例，解读一个真实项目如何组织包信息、源码、构建命令和构建依赖。为便于逐行核对，本页摘录固定到 [`2bd8bf49`](https://github.com/OpenAtom-Linyaps/linyaps/blob/2bd8bf49ec8e34c52cf6dd296829cc0e0caa610b/linglong.yaml) 提交。

下面每一节都先原封不动摘录原始配置，再解释字段和命令。由于 `master` 分支会持续更新，如果页面内容与上游发生差异，请以上游文件为准。完整字段定义见[构建配置文件简介](./manifests.md)。

## 文件头和格式版本

```yaml
# SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later
version: '1'
```

前三行是 SPDX 版权和许可证声明。`version: '1'` 表示该文件使用版本 1 的 `linglong.yaml` 配置格式，它不是应用版本。

## 软件包信息

```yaml
package:
  id: cn.org.linyaps.builder.utils
  name: ll-builder-utils
  version: 0.0.3.0
  kind: app
  description: |
    Utils for ll-builder
```

各字段含义如下：

| 字段 | 原始值 | 说明 |
| --- | --- | --- |
| `id` | `cn.org.linyaps.builder.utils` | 软件包的唯一标识，采用倒置域名格式 |
| `name` | `ll-builder-utils` | 软件包名称 |
| `version` | `0.0.3.0` | 当前配置所构建的软件包版本 |
| `kind` | `app` | 表示该软件包具有可执行入口 |
| `description` | `Utils for ll-builder` | 说明该软件包为 `ll-builder` 提供辅助工具 |

虽然 `ll-builder-utils` 不是桌面应用，但它提供可执行命令，因此使用 `kind: app`。

## 启动命令

```yaml
command: [/opt/apps/cn.org.linyaps.builder.utils/files/bin/ll-builder-export]
```

`command` 指定软件包的默认入口。这里启动 `/opt/apps/cn.org.linyaps.builder.utils/files/bin/ll-builder-export`，因此构建结果中必须存在对应的可执行文件。

`/opt/apps/cn.org.linyaps.builder.utils/files` 是该应用在运行环境中的文件目录，对应构建阶段使用的 `$PREFIX`。

## Base

```yaml
base: org.deepin.base/25.2.0
```

该配置使用 `org.deepin.base/25.2.0` 作为基础环境。Base 提供构建和运行所需的基础系统组件。

原始配置没有 `runtime` 字段，表示这个软件包不需要使用额外的 Runtime。Base 和 Runtime 的选型原则见[运行时组件](../reference/runtime.md)。

## 源码

```yaml
sources:
  - kind: archive
    url: https://github.com/erofs/erofs-utils/archive/refs/tags/v1.8.6.tar.gz
    digest: 5b221dc3fd6d151425b30534ede46fb7a90dc233a8659cba0372796b0a066547
    name: erofs-utils
  - kind: archive
    url: https://github.com/libfuse/libfuse/releases/download/fuse-3.17.1/fuse-3.17.1.tar.gz
    digest: 2d8ae87a4525fbfa1db5e5eb010ff6f38140627a7004554ed88411c1843d51b2
    name: fuse
  - kind: archive
    url: https://github.com/OpenAtom-Linyaps/linyaps-box/archive/refs/tags/2.1.2.tar.gz
    digest: 70c908e3a2397d195d64d606b886c9635b13a461beea70a6cc5317e0bb6e9589
    name: linyaps-box
```

该配置声明了三个 `archive` 类型的源码：

| `name` | URL 中的版本 | 构建阶段的用途 |
| --- | --- | --- |
| `erofs-utils` | `v1.8.6` | 构建 EROFS 和 FUSE 相关工具 |
| `fuse` | `3.17.1` | 构建静态 FUSE 库 |
| `linyaps-box` | `2.1.2` | 构建静态 `linyaps-box` |

每一项中的 `url` 是压缩包地址，`digest` 用于校验下载内容，`name` 用于确定 `/project/linglong/sources` 下的源码目录名称。

压缩包解压后仍可能包含版本目录。例如，`name: fuse` 对应的构建路径是 `/project/linglong/sources/fuse/fuse-3.17.1`。修改源码版本时，需要同步检查 URL、摘要以及 `build` 中使用的路径。

## 构建流程

构建脚本按照依赖关系依次执行以下操作。

### 输出安装前缀

```yaml
build: |
  echo "$PREFIX"

```

`echo "$PREFIX"` 输出当前应用的安装前缀，方便从构建日志确认最终产物应该安装到哪个目录。

### 构建静态 FUSE 库

```yaml
  # build libfuse static library
  cd /project/linglong/sources/fuse/fuse-3.17.1
  patch lib/mount.c /project/apps/ll-builder-utils/patch/libfuse.patch
  mkdir build || true
  cd build
  meson setup ../
  meson configure --default-library static -D utils=false -D examples=false -D tests=false -D disable-libc-symbol-version=false
  ninja && ninja install

```

1. 进入 `fuse-3.17.1` 源码目录。
2. 使用项目中的 `apps/ll-builder-utils/patch/libfuse.patch` 修改 `lib/mount.c`。
3. 创建并进入 `build` 目录。`mkdir build || true` 允许目录已存在时继续执行。
4. 使用 `meson setup ../` 初始化构建目录。
5. `meson configure` 将默认库类型设为静态库，关闭 utils、examples 和 tests，并显式设置 libc 符号版本选项。
6. `ninja && ninja install` 先完成构建，成功后再执行安装。

这一阶段生成后续组件需要的静态 FUSE 库。

### 构建 erofsfuse 静态库

```yaml
  # build erofsfuse static library
  cd /project/linglong/sources/erofs-utils/erofs-utils-1.8.6
  ./autogen.sh
  ./configure -with-libzstd --enable-fuse --enable-static-fuse --with-libdeflate --without-xxhash libdeflate_LIBS=-ldeflate libdeflate_CFLAGS=-ldeflate
  make -j$(nproc)
  make install

```

1. 进入 `erofs-utils-1.8.6` 源码目录。
2. 执行 `./autogen.sh` 生成 Autotools 构建文件。
3. 原始 `./configure` 命令启用 zstd、FUSE、静态 FUSE 和 libdeflate，关闭 xxHash，并显式设置 libdeflate 的链接与编译参数。
4. `make -j$(nproc)` 使用当前可用的 CPU 核心并行构建。
5. `make install` 安装构建结果，供后续步骤使用。

### 构建静态 linyaps-box

```yaml
  # build static ll-box
  cd /project/linglong/sources/linyaps-box/linyaps-box-2.1.2/
  cmake --preset static
  cmake --build build-static -j$(nproc)
  cmake --install build-static --prefix=$PREFIX

```

1. 进入 `linyaps-box-2.1.2` 源码目录。
2. `cmake --preset static` 使用项目提供的 `static` Preset 配置构建。
3. `cmake --build build-static -j$(nproc)` 并行构建。
4. `cmake --install build-static --prefix=$PREFIX` 将产物安装到当前应用的 `$PREFIX`。

### 构建当前 Linyaps 项目

```yaml
  cd /project
  cmake -B build-linglong -DENABLE_CPM=false -DENABLE_TESTING=false -DBUILD_LINGLONG_BUILDER_UTILS_IN_BOX=true -DAGGRESSIVE_UAB_SIZE=ON
  cmake --build build-linglong -j$(nproc)
  cmake --install build-linglong --prefix=$PREFIX
  install /usr/local/bin/mkfs.erofs $PREFIX/bin/
```

1. `cd /project` 回到当前 Linyaps 项目的源码根目录。
2. CMake 配置命令关闭 CPM 和测试，启用容器内的 Builder Utils 构建，并将 `AGGRESSIVE_UAB_SIZE` 设为 `ON`。
3. `cmake --build build-linglong -j$(nproc)` 构建当前项目。
4. `cmake --install build-linglong --prefix=$PREFIX` 将结果安装到当前应用目录。
5. `install /usr/local/bin/mkfs.erofs $PREFIX/bin/` 将之前生成的 `mkfs.erofs` 收集到最终软件包的 `bin` 目录。

其中，`command` 指向的 `ll-builder-export` 和运行所需的其他工具都必须在最终安装阶段进入 `$PREFIX`。

## 构建依赖

```yaml
buildext:
  apt:
    build_depends:
      [
        patch,
        meson,
        libtool,
        pkg-config,
        uuid-dev,
        libdeflate-dev,
        libzstd-dev,
        nlohmann-json3-dev,
        libyaml-cpp-dev,
        liblz4-dev,
        liblzma-dev,
        libselinux1-dev,
        libpcre2-dev,
        libelf-dev,
        libcap-dev,
        libcli11-dev,
        libgtest-dev,
        libsystemd-dev,
        libfmt-dev,
        libexpected-dev
      ]
```

`buildext.apt.build_depends` 声明需要在构建环境中安装的 deb 软件包：

- `patch`、`meson`、`libtool` 和 `pkg-config` 提供构建工具；
- `uuid-dev`、`libdeflate-dev`、`libzstd-dev`、`liblz4-dev` 和 `liblzma-dev` 提供文件系统及压缩相关开发文件；
- `nlohmann-json3-dev`、`libyaml-cpp-dev`、`libselinux1-dev`、`libpcre2-dev`、`libelf-dev`、`libcap-dev`、`libcli11-dev`、`libsystemd-dev`、`libfmt-dev` 和 `libexpected-dev` 提供当前项目所需的开发库；
- `libgtest-dev` 提供 GoogleTest 开发文件。

这些软件包用于构建，不会仅因为出现在 `build_depends` 中就自动成为最终应用内容。需要随应用分发的文件仍然需要通过 `build` 中的指令安装到 `$PREFIX`。

## 总结

从源码构建如意玲珑应用时，`build` 指令一般需要完成以下工作：

1. **进入源码目录**：根据 `sources.name` 和压缩包解压后的目录结构，进入需要构建的源码目录。
2. **准备源码**：按项目需要应用 Patch、生成 `configure`，或完成其他构建前准备工作。
3. **配置构建**：调用项目原有的 CMake、Meson、Autotools、qmake 等构建系统，并将安装前缀设置为 `${PREFIX}`。
4. **编译源码**：执行构建系统提供的编译命令，可以使用 `$(nproc)` 等方式进行并行构建。
5. **安装产物**：执行构建系统的安装命令，将可执行文件、库和资源安装到 `${PREFIX}`，而不是直接写入构建容器的 `/usr`。
6. **收集额外文件**：将构建系统没有自动安装、但应用运行时需要的工具、桌面文件、图标或其他资源复制到 `${PREFIX}` 的对应目录。

常见构建系统通常可以直接沿用其标准构建和安装方式。例如：

```bash
# CMake
cmake -S . -B build -DCMAKE_INSTALL_PREFIX=${PREFIX}
cmake --build build
cmake --install build

# Meson
meson setup build --prefix=${PREFIX}
meson compile -C build
meson install -C build

# Autotools
./configure --prefix=${PREFIX}
make -j$(nproc)
make install
```

因此，只要源代码支持常规构建系统的编译和安装流程，并且能够指定安装前缀，通常就可以把原有构建命令迁移到 `linglong.yaml` 的 `build` 字段中，无须为如意玲珑重写整套构建系统。迁移时主要需要补充源码声明、Base/Runtime、构建依赖和启动命令，并确保最终运行所需的文件全部进入 `${PREFIX}`。
