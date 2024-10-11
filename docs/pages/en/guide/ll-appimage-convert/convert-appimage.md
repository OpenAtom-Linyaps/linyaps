<!--
SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

## Conversion appimage application

convert `appimage` format( `.appimage` or `.AppImage` ) application to linglong format( `.layer` or `.uab` ) application

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

The `ll-appimage-convert convert` command will generate a directory according to specified app name( `--name` option), it will as a root directory of the linglong project, where the `linglong.yaml` file is located. and it supports two convert methods:

1. you can use `--file` option to convert to linglong application according to specified appimage file;
2. you can use `--url` and `--hash` option to convert to linglong application according to specified appimage url and hash value;
3. you can use `--layer` option to export `.layer`, or it will export `.uab` default.

`Tips: When the linglong version is greater than 1.5.7, the default convert package format is `.uab`, if you want to export a `.layer` file, you need to add the --layer option.`

you can use `--output` option to generate config file( `linglong.yaml` ) of linglong project and a script of build linglong `.layer`(`.uab`)
then you can execute script file to generate after you modify the `linglong.yaml` file. if you do not specify this option, it will export linglong `.layer` or linglong `.uab` directly.

Take converting [BrainWaves](https://github.com/makebrainwaves/BrainWaves/releases/download/v0.15.1/BrainWaves-0.15.1.AppImage) into linglong `.layer` through `--url` as an example, the main steps as follows:

Specify the relevant parameters of the linglong package you want to convert, you can acquire `io.github.brainwaves_0.15.1.0_x86_64_runtime.layer` or `io.github.brainwaves_0.15.1.0_x86_64_runtime.uab` wait a moment.

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
The exported `.uab` or `.layer` needs to be installed and verified. install the layer file and run the application, you can refer to: [Install the application](../ll-cli/install.md)