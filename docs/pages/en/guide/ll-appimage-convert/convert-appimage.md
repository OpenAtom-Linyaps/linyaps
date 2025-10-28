<!--
SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

## Conversion appimage application

convert `appimage` format (`.appimage` or `.AppImage`) application to linyaps format (`.layer` or `.uab`) application

View the help information for the `ll-appimage-convert convert --help` command:

```bash
ll-appimage-convert convert --help
```

Here is the output:

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

The `ll-appimage-convert convert` command will generate a directory according to the specified app name (`--name` option), which will serve as the root directory of the linyaps project where the `linglong.yaml` file is located. It supports two conversion methods:

1. you can use the `--file` option to convert to linyaps application according to the specified appimage file;
2. you can use the `--url` and `--hash` option to convert to linyaps application according to the specified appimage url and hash value;
3. you can use the `--layer` option to export `.layer`, or it will export `.uab` by default.

`Tips: When the linyaps version is greater than 1.5.7, the default converted package format is `.uab`. If you want to export a `.layer` file, you need to add the --layer option.`

you can use the `--output` option to generate the configuration file (`linglong.yaml`) of the linyaps project and a script for building linyaps `.layer` (`.uab`)
then you can execute the script file to generate after you modify the `linglong.yaml` file. If you do not specify this option, it will export linyaps `.layer` or `.uab` directly.

Take converting [BrainWaves](https://github.com/makebrainwaves/BrainWaves/releases/download/v0.15.1/BrainWaves-0.15.1.AppImage) into linyaps `.layer` through `--url` as an example, the main steps are as follows:

Specify the relevant parameters of the linyaps package you want to convert, and you will obtain `io.github.brainwaves_0.15.1.0_x86_64_runtime.layer` or `io.github.brainwaves_0.15.1.0_x86_64_runtime.uab` after a short wait.

```bash
ll-appimage-convert convert --url "https://github.com/makebrainwaves/BrainWaves/releases/download/v0.15.1/BrainWaves-0.15.1.AppImage" --hash "04fcfb9ccf5c0437cd3007922fdd7cd1d0a73883fd28e364b79661dbd25a4093" --name "io.github.brainwaves" --id "io.github.brainwaves" --version "0.15.1.0" --description "io.github.brainwaves" -b
```

Take converting BrainWaves-0.15.1.AppImage into `.uab` through `--file` as an example, the main steps as follows:

```bash
ll-appimage-convert convert -f ~/Downloads/BrainWaves-0.15.1.AppImage --name "io.github.brainwaves" --id "io.github.brainwaves" --version "0.15.1.0" --description "io.github.brainwaves" -b
```

The converted directory structure is as follows:

```text
├── io.github.brainwaves_x86_64_0.15.1.0_main.uab
├── linglong
└── linglong.yaml
```

Take converting BrainWaves-0.15.1.AppImage into `.layer` through `--file` as an example, the main steps as follows:

```bash
ll-appimage-convert convert -f ~/Downloads/BrainWaves-0.15.1.AppImage --name "io.github.brainwaves" --id "io.github.brainwaves" --version "0.15.1.0" --description "io.github.brainwaves" -b --layer
```

The converted directory structure is as follows:

```text
├── io.github.brainwaves_0.15.1.0_x86_64_binary.layer
├── io.github.brainwaves_0.15.1.0_x86_64_develop.layer
├── linglong
└── linglong.yaml
```

`.uab` or `.layer` file verification
The exported `.uab` or `.layer` files need to be installed and verified. Install the layer files and run the application. You can refer to: [Install the application](../ll-cli/install.md)
