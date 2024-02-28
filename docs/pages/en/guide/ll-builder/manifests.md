<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# Manifests

`linglong.yaml` is the description file of a Linglong project, which stores the relevant information required for building. Such as the name, version, source address, build dependencies, etc., of the build product.

## Project directory structure

```bash
{project-root}
├── linglong.yaml
└── .linglong-target

{user-home}
└── .cache
    └── linglong-builder
        ├── repo
        └── layers
```

## Field definitions

### App meta info

```yaml
package:
  id: org.deepin.calculator
  name: deepin-calculator
  version: 5.7.21
  kind: app
  description: |
    calculator for deepin os.
```

| name        | description                                                                                          |
| ----------- | ---------------------------------------------------------------------------------------------------- |
| description | Detailed description of the build product                                                            |
| id          | Unique name of the build product                                                                     |
| kind        | The type of build product: app, runtime, lib, representing application, library in turn |
| version     | version of the build product                                                                         |

### Runtime

Describes the build and run dependencies of the application.

```yaml
runtime:
  id: org.deepin.Runtime
  version: 23.0.0
  # NOT IMPLEMENTATION NOW
  digest: 4d85525f09211381c77d2085c9c1057
```

The `id` can also be written to include the version as:

```text
runtime:
  id: org.deepin.Runtime/20.5.0
```

| name    | description                                                                             |
| ------- | --------------------------------------------------------------------------------------- |
| id      | Unique name of the runtime                                                              |
| version | Runtime version                                                                         |
| digest  | (not used yet, this field can be used to bind a unique version of the runtime) |

### Dependencies

Describes the build dependencies and runtime dependencies of the application.

```yaml
depends:
  - id: dde-qt-dbus-factory
    version: 5.5.12
  - id: googletest
    version: 1.8.1
  - id: icu
    version: 63.1.0
    type: runtime
  - id: xcb-util
    version: 0.3.8.1
    type: runtime
```

| name    | description                                                                                          |
| ------- | ---------------------------------------------------------------------------------------------------- |
| id      | Unique name of the dependency                                                                        |
| type    | The type of dependency, the type of runtime dependency, will be submitted with the build content |
| version | Dependency version                                                                                    |
| digest  | (not used yet, this field can be used to bind a unique version of the dependency)                    |

`ll-builder` will pull dependencies from the remote repository to the local when the project is built. If the dependency does not exist in the remote repository or cannot meet the requirements, you can add source and build content, and `ll-builder` will build and apply this type of dependency first to the project build.

```yaml
depends: 
  - id: icu
    version: 63.1.0
    source:
      kind: git
      url: https://github.com/deepin-community/icu.git
      version: master
      commit: 3c651bcc40748fc5d02d9134fcaee14fda44ab62
    build:
      kind: autotools
```

### Source

Describes the source information.

```yaml
source:
  kind: git
  url: https://github.com/linuxdeepin/deepin-calculator.git
  version: master
  commit: d7e207b4a71bbd97f7d818de5044228c1a6e2c92
  patch:
    -patches/fix-install-prefix-path.patch
    - patches/fix-lib-install-path.patch
```

| name    | description                                                          |
| ------- | -------------------------------------------------------------------- |
| kind    | Source code type, optional types local, archive, git                 |
| url     | Source address, fill in when the type is archive or git              |
| version | Source branch version, fill in when the type is git                  |
| digest  | The Hash value of archive file encrypted using sha256 algorithm, fill in when the type is git          |
| commit  | The hash value of a source code commit, fill in when the type is git |
| patch   | Source patch path                                                    |

### Build rules

Describes the build rules.

```yaml
build:
  kind: manual
  manual:
    configure: |
      mkdir build
      cd build
      qmake -r PREFIX=${PREFIX} ..
    build: |
      make -j
      make test
    install: |
      make -j install
```

```yaml
build:
  kind: autotools
  manual:
    configure: |
      ./bootstrap.sh
```

| name      | description                                                                                               |
| --------- | --------------------------------------------------------------------------------------------------------- |
| build     | build rules at build time                                                                                 |
| configure | build-time configure rules                                                                                |
| install   | build-time install rules                                                                                  |
| kind      | Build type, optional manual, autotools, cmake, qmake                                                      |
| manual    | Build rules, when manual is declared, it means custom rules, that is, rewriting build, install, configure |

### Variables

Describes the variables that can be used by the build.

```yaml
variables:
  build_dir: |
  dest_dir: |
  conf_args: |
    --prefix=${PREFIX}
    --libdir=lib/${TRIPLET}
  extra_args: |
    --doc=enable
  jobs: |
    -j64

build:
  kind: manual
  manual:
    configure: |
      ./configure ${conf_args} ${extra_args}
    build: |
      make ${jobs}
    install: |
      make DESTDIR=${dest_dir} install
```

| name       | description                                                                                                                                                             |
| ---------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| build_dir  | Built-in variables, assigned in the variable field, used in the build field                                                                                             |
| dest_dir   | Same as build_dir                                                                                                                                                       |
| conf_args  | Same as build_dir                                                                                                                                                       |
| extra_args | Same as build_dir                                                                                                                                                       |
| jobs       | Same as build_dir                                                                                                                                                       |
| PREFIX     | One of the environment variables, which can be used under the variable and build fields; provide the installation path when building                                    |
| TRIPLET    | One of the environment variables, which can be used under the variable and build fields; provide a triple containing architecture information, such as x86_64-linux-gnu |

## Complete example

### Build app

```yaml
package:
  id: org.deepin.calculator
  name: deepin-calculator
  version: 5.7.21
  kind: app
  description: |
    calculator for deepin os.

runtime:
  id: org.deepin.Runtime
  version: 23.0.0

depends:
  - id: dde-qt-dbus-factory
    version: 5.5.12
  - id: googletest
    version: 1.8.1
  - id: icu
    version: 63.1.0
    type: runtime
  - id: xcb-util
    version: 0.3.8.1
    type: runtime

source:
  kind: git
  url: https://github.com/linuxdeepin/deepin-calculator.git
  version: master
  commit: d7e207b4a71bbd97f7d818de5044228c1a6e2c92

variables:
  extra_args: |
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_SAFETYTEST_ARG="CMAKE_SAFETYTEST_ARG_OFF" \
    -DAPP_VERSION=5.7.21 \
    -DVERSION=5.7.21

build:
  kind: cmake
```

### Build dependencies

```yaml
package:
   id: libopenjp2
   kind: lib
   version: 2.4.0

base:
   id: org.deepin.base
   version: 23.0.0

source:
   kind: git
   url: https://github.com/uclouvain/openjpeg
   version: 2.4.0
   commit: 37ac30ceff6640bbab502388c5e0fa0bff23f505

build:
   kind: cmake
```

### Build runtime

```yaml
package:
   id: org.deepin.Runtime
   kind: runtime
   version: 23.0.0
   description: |
     runtime of deepin.

base:
   id: org.deepin.base/23.0.0

depends:
  - id: qtbase/5.15.7
  - id: qttranslations/5.15.7
  - id: qt3d/5.15.7
  - id: qtcharts/5.15.7
  - id: qtconnectivity/5.15.7
  - id: qtgamepad/5.15.7
  - id: qtsensors/5.15.7
  - id: qtspeech/5.15.7
  - id: qtvirtualkeyboard/5.15.7
  - id: qtserialport/5.15.7
  - id: qtnetworkauth/5.15.7
  - id: qttools/5.15.7
  - id: qtx11extras/5.15.7
  - id: qtdeclarative/5.15.7
  - id: qtsvg/5.15.7
  - id: qtscript/5.15.7
  - id: qtgraphicaleffects/5.15.7
  - id: qtquickcontrols/5.15.7
  - id: qtquickcontrols2/5.15.7
  - id: qtxmlpatterns/5.15.7
  - id: qtwayland/5.15.7
  - id: qtmultimedia/5.15.7
  - id: qtwebchannel/5.15.7
  - id: qtwebsockets/5.15.7
  - id: qtimageformats/5.15.7
  - id: qtlocation/5.15.7
  - id: spdlog/1.10.0
  - id: fmtlib/10.1.2
  - id: dtkcommon/5.6.0.3
  - id: dtkcore/5.6.0.9
  - id: dtkgui/5.6.0.13
  - id: dtkwidget/5.6.0.6
  - id: dtkdeclarative/5.6.0.7
  - id: qt5integration/5.6.0.5
  - id: qt5platform-plugins/5.6.0.5
  - id: libqtxdg/3.6.0.1
  - id: double-conversion/3.1.0.3
  - id: deepin-shortcut-viewer/5.0.6.1
  - id: fcitx-qt5/1.2.6.6
  - id: gsettings-qt/0.3.1.1
  - id: linglong-config/0.0.1.2

build:
   kind: manual
   manual:
     configure: |
       echo skip configure
```

When the package type is runtime, all dependencies will be submitted.

## Build tools template

### Automake type build template

`autotools.yaml` provides a generic `automake` type build template, the template file is as follows:

```yaml
variables:
   build_dir: build_dir
   conf_args: |
     --prefix=${PREFIX} \
     --libdir=${PREFIX}/lib/${TRIPLET}
   extra_args: |
   dest_dir: |
   jobs: -j${JOBS}

build:
   kind: autotools
   manual:
     configure: |
       #autogon.sh, bootstrap.sh
       autoreconf -ivf
       ./configure ${conf_args} ${extra_args}
     build: |
       make ${jobs}
     install: |
       make ${jobs} DESTDIR=${dest_dir} install
```

Build tool:

```yaml
build:
   kind: autotools
```

### qmake build template

`qmake.yaml` provides a generic `qmake` build template, the template file is as follows:

```yaml
variables:
   build_dir: build_dir
   conf_args: |
     PREFIX=${PREFIX} \
     LIB_INSTALL_DIR=${PREFIX}/lib/${TRIPLET}
   extra_args: |
   dest_dir: |
   jobs: -j${JOBS}

build:
   kind: qmake
   manual :
     configure: |
       qmake -makefile ${conf_args} ${extra_args}
     build: |
       make ${jobs}
     install: |
       make ${jobs} DESTDIR=${dest_dir} install
```

Build tool:

```yaml
build:
   kind: qmake
```

### cmake build templates

`cmake.yaml` provides a generic `cmake` build template, the template file is as follows:

```yaml
variables:
   build_dir: build_dir
   conf_args: |
     -DCMAKE_INSTALL_PREFIX=${PREFIX} \
     -DCMAKE_INSTALL_LIBDIR=${PREFIX}/lib/${TRIPLET}
   extra_args: |
   dest_dir: |
   jobs: -j${JOBS}

build:
   kind: cmake
   manual :
     configure: |
       cmake -B ${build_dir} ${conf_args} ${extra_args}
     build: |
       cmake --build ${build_dir} -- ${jobs}
     install: |
       env DESTDIR=${dest_dir} cmake --build ${build_dir} --target install
```

Build tool:

```yaml
build:
   kind: cmake
```

## Runtime 23.0.0 includes these dependencies:

| id                     | version   |
| ---------------------- | --------- |
| qtbase                 | 5.15.7    |
| qt3d                   | 5.15.7    |
| qtcharts               | 5.15.7    |
| qtconnectivity         | 5.15.7    |
| qtgamepad              | 5.15.7    |
| qtsensors              | 5.15.7    |
| qtspeech               | 5.15.7    |
| qtvirtualkeyboard      | 5.15.7    |
| qtserialport           | 5.15.7    |
| qtnetworkauth          | 5.15.7    |
| qttools                | 5.15.7    |
| qtx11extras            | 5.15.7    |
| qtdeclarative          | 5.15.7    |
| qtsvg                  | 5.15.7    |
| qtscript               | 5.15.7    |
| qtgraphicaleffects     | 5.15.7    |
| qtquickcontrols        | 5.15.7    |
| qtquickcontrols2       | 5.15.7    |
| qtxmlpatterns          | 5.15.7    |
| qtwayland              | 5.15.7    |
| qtmultimedia           | 5.15.7    |
| qtwebchannel           | 5.15.7    |
| qtwebsockets           | 5.15.7    |
| qtimageformats         | 5.15.7    |
| qtlocation             | 5.15.7    |
| dtkcommon              | 5.6.0.3   |
| dtkcore                | 5.6.0.9   |
| dtkgui                 | 5.6.0.13  |
| dtkwidget              | 5.6.0.6   |
| dtkdeclarative         | 5.6.0.7   |
| libqtxdg               | 3.6.0.3   |
| qt5integration         | 5.6.0.5   |
| double-conversion      | 3.1.0.2   |
| deepin-shortcut-viewer | 5.0.6.1   |
| fcitx-qt5              | 1.2.6.6   |
| gsettings-qt           | 0.3.1.1   |
| qt5platform-plugins    | 5.6.0.5   |
| spdlog                 | 1.10.0    |
| fmtlib                 | 10.1.2    |

## Other available dependencies:

| id                   | version    |
| -------------------- | ---------- |
| aria2                | 1.34.0     |
| bzip2                | 1.0.6      |
| cdparanoia           | 3.10.2     |
| dde-control-center   | 5.5.23     |
| dde-dock             | 5.5.15     |
| disomaster           | 5.0.7      |
| djvulibre            | 3.5.27.1   |
| ffmpeg               | 4.1.8      |
| ffmpegthumbnailer    | 2.1.1      |
| freeimage            | 3.18.0     |
| gio-qt               | 0.0.11     |
| gst-plugins-good1.0  | 1.14.5     |
| gstreamer1.0         | 1.14.6     |
| ilmbase              | 2.2.1      |
| image-editor         | 1.0.9      |
| jxrlib               | 1.1.1      |
| karchive             | 5.54.0     |
| kcodecs              | 5.54.0     |
| kconfig              | 5.54.0     |
| kcoreaddons          | 5.54.0     |
| kcrash               | 5.54.0     |
| kdbusaddons          | 5.54.0     |
| kglobalaccel         | 5.54.0     |
| ki18n                | 5.54.0     |
| kservice             | 5.54.0     |
| ksyntax-highlighting | 5.54.0     |
| kwayland             | 5.57.0     |
| kwindowsystem        | 5.54.0     |
| leveldb              | 1.20       |
| libarchive           | 3.3.3      |
| libchardet           | 1.0.4      |
| libdbusmenu-qt       | 0.9.3      |
| libdmr               | 5.9.13     |
| libdv                | 1.0.0      |
| libepoxy             | 1.5.3      |
| libevent             | 2.1.8      |
| libraw               | 0.19.2     |
| libvisual            | 0.4.0      |
| libxrandr            | 1.5.1      |
| libxtst              | 5.5.15     |
| libzip               | 1.5.1.8    |
| minizip              | 1.1.1      |
| mpv                  | 0.29.1.37  |
| opencv               | 3.2.0      |
| openexr              | 2.2.1.4    |
| procps               | 3.3.15     |
| qtdbusextended       | 0.0.3      |
| qtmpris              | 0.1.0.1    |
| sqlite3              | 3.27.2.3   |
| taglib               | 1.11.1     |
| uchardet             | 0.0.6      |
| udisks2-qt5          | 5.0.3      |
| vlc                  | 3.0.11     |
| xcb-util             | 0.3.8.1    |
| icu                  | 63.1       |
| dde-qt-dbus-factory  | 5.5.12     |
| gst-plugins-base1.0  | 1.14.4.2   |
| libdvdnav            | 6.0.0      |
| libdvdread           | 6.0.0      |
| libical3             | 3.0.4      |
| libva                | 2.4.0      |
| libxcursor           | 1.1.15     |
| orc                  | 0.4.28     |
| poppler              | 0.71.0.2   |
| kmod                 | 26.1       |
| pciutils             | 3.5.2.4    |
| xz-utils             | 5.2.4.1    |
| libxslt              | 1.1.32.3   |
| v4l-utils            | 1.20.0     |
| portaudio19          | 19.6.0     |
| gst-plugins-bad1.0   | 1.14.4.2   |
| gst-plugins-ugly1.0  | 1.14.4.2   |
| nettle               | 3.4.1.2    |
| aom                  | 1.0.0.1    |
| codec2               | 0.8.1      |
| libdc1394            | 2.2.5      |
| libmysofa            | 0.6        |
| x264                 | 0.155.2917 |
| x265                 | 2.9        |
| xavs2                | 1.4.1      |
| cunit                | 2.1        |
| lapack               | 3.8.0.1    |
| openblas             | 0.3.5      |
| crystalhd            | 0.0.1      |
| davs2                | 1.7.1      |
| elfutils             | 0.176.2    |
