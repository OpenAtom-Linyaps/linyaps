<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# 导出

`ll-builder export` 命令用于将本地构建缓存中的应用导出为 UAB (Universal Application Bundle) 文件。这是推荐的格式。也可以导出为已弃用的 linglong layer 文件格式。

## 基本用法

```bash
ll-builder export [OPTIONS]
```

要查看所有可用选项及其详细说明，请运行：
```bash
ll-builder export --help
```

## 主要选项

*   `-f, --file FILE`: 指定 `linglong.yaml` 配置文件的路径 (默认: `./linglong.yaml`)，`linglong.yaml` 文件所在的目录为项目的工作目录。
*   `-o, --output FILE`: 指定输出文件的路径。对于 UAB，这通常是 `.uab` 文件的完整路径或文件名。对于 layer 这是输出文件名的前缀。
*   `-z, --compressor X`: 指定压缩算法。支持 `lz4` (UAB 默认), `lzma` (layer 默认), `zstd`。
*   `--icon FILE`: 为导出的 UAB 文件指定图标 (仅 UAB 模式，与 `--layer` 互斥)。
*   `--loader FILE`: 为导出的 UAB 文件指定自定义加载器 (仅 UAB 模式，与 `--layer` 互斥)。
*   `--layer`: **(已弃用)** 导出为 layer 文件格式，而不是 UAB (与 `--icon`, `--loader` 互斥)。
*   `--no-develop`: 在导出 layer 文件时，不包含 `develop` 模块。

## 导出 UAB 文件 (推荐)

UAB (Universal Application Bundle) 文件一种自包含、可离线分发的文件格式，包含了应用运行所需的内容。这是推荐的导出格式。

`ll-builder export` 命令执行后会在项目工作目录（或使用 `-o` 指定的路径）生成一个 `.uab` 文件，例如 `<app_id>_<version>_<arch>.uab`。

可以使用其他选项自定义 UAB 导出：

```bash
# 导出 UAB 文件并指定图标和输出路径
ll-builder export --icon assets/app.png -o dist/my-app-installer.uab

# 导出 UAB 文件并使用 zstd 压缩
ll-builder export -z zstd -o my-app-zstd.uab

# 导出 UAB 文件并指定自定义加载器
ll-builder export --loader /path/to/custom/loader -o my-app-custom-loader.uab
```

## 导出 layer 文件 (已弃用)

**注意：导出 layer 文件格式已被弃用，推荐使用 UAB 格式。**

通过命令 `ll-builder export --layer` 导出 layer 文件，其他示例：

```bash
# 导出 layer 格式，且不包含 develop 模块
ll-builder export --layer --no-develop

# 导出 layer 格式，并指定输出文件前缀
ll-builder export --layer -o my-app
# (会生成 my-app_binary.layer 和 my-app_develop.layer)
```

导出 layer 文件会生成以下文件：
*   `*_binary.layer`: 包含应用运行所需的基本文件。
*   `*_develop.layer`: （可选）包含用于开发和调试的文件。如果使用了 `--no-develop` 选项，则不会生成此文件。

## 进阶说明

UAB 是一个静态链接的 ELF 可执行文件，其目标是可以在任意 Linux 发行版上运行。默认情况下导出 UAB 文件属于 bundle 模式，而使用参数 `--loader` 导出 UAB 文件属于 custom loader 模式。

### Bundle 模式

bundle 模式主要用于分发，并尽可能的支持自运行功能。通常用户通过 `ll-cli install <UABfile>` 安装离线分发的应用，并会自动补全应用所需的运行环境。安装后的 UAB 应用，使用方法和直接从仓库安装的应用一样。

bundle 模式在导出时，会尝试自动解析应用的依赖，并导出必要的内容（尽可能保证）。如果给 bundle 模式的 UAB 文件添加可执行权限并运行，应用不会被安装而是直接运行。此时应用仍然在独立的容器内运行，但是缺少的部分运行环境会直接使用宿主机的环境，因此无法保证应用一定能运行。

如果应用开发者需要依赖自运行功能，请保证应用带上必要的运行时依赖。

### Bustom Loader 模式

custom loader 模式导出的 UAB 文件仅包含应用数据，以及传入的 custom loader。UAB 文件在解压挂载后将控制器交给 custom loader，此时 loader 不在容器环境内。环境变量 `LINGLONG_UAB_APPROOT` 保存了应用所在目录，custom loader 负责初始化应用程序所需要的运行环境，比如库路径的搜索。

此模式下应用程序需要保证其本身在目标系统的兼容性。推荐做法为在想要支持的最低版本的系统上构建出应用，并带上所有依赖项。
