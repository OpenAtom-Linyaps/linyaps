<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# convert `appimage` format( `.appimage` or `.AppImage` ) pkg to linglong format( `.layer` or `.uab` ) pkg

Use `ll-builder convert` to convert `appimage` format( `.appimage` or `.AppImage` ) pkg to linglong format( `.layer` or `.uab` ) pkg.

View the help information for the `ll-builder convert` command:

```bash
ll-builder convert --help
```

Here is the output:

```text
Usage: apps/ll-builder/ll-builder [options] convert

Options:
  -v, --verbose                              show detail log
  -h, --help                                 Displays help on commandline
                                             options.
  --help-all                                 Displays help including Qt
                                             specific options.
  -f, --file <*.deb,*.AppImage(*.appimage)>  app package file, it not required option,
                                             you can ignore this option when you
                                             set -u option
  -u, --url <pkg url>                        pkg url, it not required option,
                                             you can ignore this option when you
                                             set -f option
  --hs, --hash <pkg hash value>              pkg hash value, it must be used
                                             with --url option
  -i, --id <app id>                          the unique name of the app
  -n, --name <app description>               the description the app
  -V, --version <app version>                the version of the app
  -d, --description <app description>        detailed description of the app
  --icon <path>                              uab icon (optional)
  -l, --layer                                export layer file
  -o, --output <script name>                 not required option, it will
                                             generate linglong.yaml and
                                             script,you can modify
                                             linglong.yaml,then enter the
                                             directory(app name) and execute the
                                             script to generate the linglong
                                             .layer(.uab)

Arguments:
  convert                                    convert app with (deb,AppImage(appimage)) format to
                                             linglong format, the default format is uab
                                             if you want to export a layer file
                                             you can use the --layer option
                                             you can generate convert config file by use -o option
```

The `ll-builder convert` command will generate a directory according to specified app name( `--name` option), it will as a root directory of the linglong project, where the `linglong.yaml` file is located. and it supports two convert methods:

1. you can use `--file` option to convert to linglong pkg according to specified appimage file;
2. you can use `--url` and `--hash` option to convert to linglong pkg according to specified appimage url and hash value;
3. you can use `--layer` option to export `.layer`, or it will export `.uab` default.

`Tips: When the linglong version is greater than 1.5.7, the default convert package format is `.uab`, if you want to export a `.layer` file, you need to add the --layer option.`

you can use `--output` option to generate config file( `linglong.yaml` ) of linglong project and a script of build linglong `.layer`(`.uab`)
then you can execute script file to generate after you modify the `linglong.yaml` file. if you do not specify this option, it will export linglong `.layer` or linglong `.uab` directly.

Take converting [BrainWaves](https://github.com/makebrainwaves/BrainWaves/releases/download/v0.15.1/BrainWaves-0.15.1.AppImage) into linglong `.layer` through `--url` as an example, the main steps as follows:

Specify the relevant parameters of the linglong package you want to convert, you can acquire `io.github.brainwaves_0.15.1_x86_64_runtime.layer` or `io.github.brainwaves_0.15.1_x86_64_runtime.uab` wait a moment.

```bash
ll-builder convert --url "https://github.com/makebrainwaves/BrainWaves/releases/download/v0.15.1/BrainWaves-0.15.1.AppImage" --hash "04fcfb9ccf5c0437cd3007922fdd7cd1d0a73883fd28e364b79661dbd25a4093" --name "io.github.brainwaves" --id "io.github.brainwaves" --version "0.15.1" --description "io.github.brainwaves" -v
```

Take converting BrainWaves-0.15.1.AppImage into `.uab` through `--file` as an example, the main steps as follows:

```bash
ll-builder convert -f ~/Downloads/BrainWaves-0.15.1.AppImage --name "io.github.brainwaves" --id "io.github.brainwaves" --version "0.15.1" --description "io.github.brainwaves"
```

The converted directory structure is as follows:
```text
io.github.brainwaves
└── io.github.brainwaves_x86_64_0.15.1.0_main.uab
```

Take converting BrainWaves-0.15.1.AppImage into `.layer` through `--file` as an example, the main steps as follows:

```bash
ll-builder convert -f ~/Downloads/BrainWaves-0.15.1.AppImage --name "io.github.brainwaves" --id "io.github.brainwaves" --version "0.15.1" --description "io.github.brainwaves" --layer
```

The converted directory structure is as follows:
```text
io.github.brainwaves
├── io.github.brainwaves_0.15.1.0_x86_64_binary.layer
└── io.github.brainwaves_0.15.1.0_x86_64_develop.layer
```

`.uab` or `.layer` file verification
The exported `.uab` or `.layer` needs to be installed and verified. install the layer file and run the application, you can refer to: [Install the application](../ll-cli/install.md)
