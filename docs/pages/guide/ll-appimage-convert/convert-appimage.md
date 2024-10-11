<!--
SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

## 转换appimage应用

转换 `appimage` 包格式( `.appimage` 或 `.AppImage` ) 到玲珑包格式( `.layer` 或 `.uab` )

查看`ll-appimage-convert convert` 命令的帮助信息：

```bash
ll-appimage-convert convert --help
```

`ll-appimage-convert convert` 命令的帮助信息如下：

```text
Usage:
  ll-appimage-convert convert [flags]
Flags:
  -b, --build                build linglong
  -d, --description string   detailed description of the package
  -f, --file string          app package file, it not required option,
                             you can ignore this option
                             when you set --url option and --hash option
      --hash string          pkg hash value, it must be used with --url option
  -h, --help                 help for convert
  -i, --id string            the unique name of the package
  -l, --layer                export layer file
  -n, --name string          the description the package
  -u, --url string           pkg url, it not required option,you can ignore this option when you set -f option
  -v, --version string       the version of the package
Global Flags:
  -V, --verbose   verbose output
```

`ll-appimage-convert convert` 命令会根据指定的应用名称（ `--name` 选项）生成一个目录，该目录会作为玲珑项目的根目录，即 `linglong.yaml` 文件所在的位置。它支持两种转换方法：

1. 你可以使用 `--file` 选项将指定的 `appimage` 文件转换为玲珑包文件；
2. 你可以使用 `--url` 和 `--hash` 选项将指定的 `appimage url` 和 `hash` 值转换为玲珑包文件;
3. 你可以使用 `--layer` 选项导出 `.layer` 格式文件，否则将默认导出 `.uab` 格式文件。

`Tips: 在玲珑版本大于1.5.7时，convert 默认导出 uab 包，如果想要导出 layer 文件，需要加上 --layer 参数`

你可以使用 `--output` 选项生成玲珑项目的配置文件( `linglong.yaml` )和构建玲珑 `.layer` ( `.uab` )的脚本文件
然后你可以执行该脚本去生成对应的玲珑包当你修改 `linglong.yaml` 配置文件后。如果不指定该选项，将直接导出对应的玲珑包。

以通过 `--url` 选项将 [BrainWaves](https://github.com/makebrainwaves/BrainWaves/releases/download/v0.15.1/BrainWaves-0.15.1.AppImage) `appimage` 文件转换为玲珑 `.layer` 文件为例，主要步骤如下：

指定要转换的玲珑包的相关参数，稍等片刻后你就可以得到 `io.github.brainwaves_0.15.1.0_x86_64_runtime.layer` 或者 `io.github.brainwaves_0.15.1.0_x86_64_runtime.uab` 包文件。

```bash
ll-appimage-convert convert --url "https://github.com/makebrainwaves/BrainWaves/releases/download/v0.15.1/BrainWaves-0.15.1.AppImage" --hash "04fcfb9ccf5c0437cd3007922fdd7cd1d0a73883fd28e364b79661dbd25a4093" --name "io.github.brainwaves" --id "io.github.brainwaves" --version "0.15.1.0" --description "io.github.brainwaves" -b
```

以通过 `--file` 选项将 `BrainWaves-0.15.1.AppImage` 转换为玲珑 `.uab` 为例，主要步骤如下：

```bash
ll-appimage-convert convert -f ~/Downloads/BrainWaves-0.15.1.AppImage --name "io.github.brainwaves" --id "io.github.brainwaves" --version "0.15.1.0" --description "io.github.brainwaves" -b
```

转换完成的目录结构如下:
```text
├── io.github.brainwaves_x86_64_0.15.1.0_main.uab
├── linglong
└── linglong.yaml
```

以通过 `--file` 选项将 `BrainWaves-0.15.1.AppImage` 转换为玲珑 `.layer` 为例，主要步骤如下：
```bash
ll-appimage-convert convert -f ~/Downloads/BrainWaves-0.15.1.AppImage --name "io.github.brainwaves" --id "io.github.brainwaves" --version "0.15.1.0" --description "io.github.brainwaves" -b --layer
```

转换完成的目录结构如下:
```text
├── io.github.brainwaves_0.15.1.0_x86_64_binary.layer
├── io.github.brainwaves_0.15.1.0_x86_64_develop.layer
├── linglong
└── linglong.yaml
```

`.uab` 或 `.layer` 文件验证
导出的`.uab`或者`.layer`需要安装后进行验证，安装 layer 文件和运行应用参考：[安装应用](../ll-cli/install.md)