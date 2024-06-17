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
└── linglong

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
  version: 5.7.21.0
  kind: app
  description: |
    calculator for deepin os.
```

| name        | description                                                                   |
| ----------- | ----------------------------------------------------------------------------- |
| description | Detailed description of the build product                                     |
| id          | Unique name of the build product                                              |
| kind        | The type of build product: app, runtime, representing Application and Runtime |
| version     | version of the build product, require a four-digit number.                    |

### Base

The minimum root filesystem.

| name    | description                                                                             |
| ------- | --------------------------------------------------------------------------------------- |
| id      | Unique name of the base                                                                 |
| version | Base version, A three-digit number can be loosely matched with a potential fourth digit |

### Runtime

Describes the build and run dependencies of the application.

```text
runtime: org.deepin.Runtime/23.0.1
```

| name    | description                                                                                |
| ------- | ------------------------------------------------------------------------------------------ |
| id      | Unique name of the runtime                                                                 |
| version | Runtime version, A three-digit number can be loosely matched with a potential fourth digit |

### Source

Describes the source information.

```yaml
sources:
  kind: git
  url: https://github.com/linuxdeepin/deepin-calculator.git
  version: master
  commit: d7e207b4a71bbd97f7d818de5044228c1a6e2c92
```

| name   | description                                                                                               |
| ------ | --------------------------------------------------------------------------------------------------------- |
| kind   | Source code type, optional types git, file, archive, git                                                  |
| url    | Source address, fill in when the type is archive or git                                                   |
| digest | The Hash value of archive file encrypted using sha256 algorithm, fill in when the type is archive or file |
| commit | The hash value of a source code commit, fill in when the type is git                                      |

### Build rules

Describes the build rules.

```yaml
build: |
  qmake -makefile PREFIX=${PREFIX} LIB_INSTALL_DIR=${PREFIX}/lib/${TRIPLET}
  make
  make install
```

| name  | description               |
| ----- | ------------------------- |
| build | build rules at build time |

### Variables

Describes the variables that can be used by the build.

| name    | description                                                                                                                                                                          |
| ------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| PREFIX  | One of the environment variables, which can be used under the variable and build fields; provide the installation path when buildingTRIPLET, such as /opt/apps/org.deepin.calculator |
| TRIPLET | One of the environment variables, which can be used under the variable and build fields; provide a triple containing architecture information, such as x86_64-linux-gnu              |

## Complete example

### Build Root FileSystem

```bash
git clone git@github.com:linglongdev/org.deepin.foundation.git
cd org.deepin.foundation
bash build_base.sh eagle amd64
```

The project is designed to construct the root filesystem utilized by Linglong, with "eagle" referring to the codename of the distribution and "amd64" indicating the architecture.

| Distribution version | Arch                      |
| -------------------- | ------------------------- |
| eagle (UOS 20)       | amd64、arm64、loongarch64 |
| beige (deepin 23)    | amd64、arm64              |

### Build runtime

```yaml
git clone git@github.com:linglongdev/org.deepin.Runtime.git -b v23
cd org.deepin.Runtime
./depend-deb-list.sh | ./tools/download_deb_depend.bash
ll-builder build --skip-fetch-source
```

On the basis of the rootfs, add fundamental environments such as Qt.

### Build app

#### calculator

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
  - /opt/apps/org.deepin.demo/files/bin/deepin-calculator

base: org.deepin.foundation/23.0.0
runtime: org.deepin.Runtime/23.0.1

sources:
  - kind: git
    url: https://github.com/linuxdeepin/deepin-calculator.git
    version: master
    commit: d7e207b4a71bbd97f7d818de5044228c1a6e2c92

  - kind: git
    url: https://github.com/linuxdeepin/dde-qt-dbus-factory.git
    version: master

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
        -DCMAKE_INSTALL_LIBDIR=${PREFIX}/lib/${TRIPLET}
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_SAFETYTEST_ARG="CMAKE_SAFETYTEST_ARG_OFF" \
        -DAPP_VERSION=5.7.21 \
        -DVERSION=5.7.21

  cmake --build build
  cmake --build build --target install
```
