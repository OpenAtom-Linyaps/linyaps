<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# Introduction to Build Configuration Files

`linglong.yaml` is the description file for a linyaps project, recording relevant information required for building, such as the name, version, source address, and build dependencies of the build artifact.

## Project Directory Structure

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

## Field Definitions

The `linglong.yaml` file structure follows specific specifications. First, you need to declare the configuration file version at the top level:

```yaml
version: '1'
```

| Name    | Description                                | Required |
| ------- | ------------------------------------------ | -------- |
| version | The version of the build configuration file, currently '1' | Yes      |

Next are the main configuration blocks. Among them, `package`, `base`, and `build` must be defined.

### Package Metadata Configuration (`package`)

Defines the basic information of the build artifact. This `package` configuration block is required.

```yaml
package:
  id: org.deepin.calculator
  name: deepin-calculator
  version: 5.7.21.0 # Four-digit version number recommended
  kind: app # or runtime
  description: |
    calculator for deepin os.
  architecture: amd64 # Optional
  channel: stable    # Optional
```

| Name           | Description                                                     | Required |
| -------------- | --------------------------------------------------------------- | -------- |
| id             | Unique name of the build artifact (e.g., `org.deepin.calculator`) | Yes      |
| name           | Name of the build artifact (e.g., `deepin-calculator`)          | Yes      |
| version        | Version of the build artifact, four digits recommended (e.g., `5.7.21.0`) | Yes      |
| kind           | Type of the build artifact: `app` (Application), `runtime` (Runtime) | Yes      |
| description    | Detailed description of the build artifact                      | Yes      |
| architecture   | Target architecture of the build artifact (e.g., `amd64`, `arm64`) | No       |
| channel        | Channel of the build artifact (e.g., `stable`, `beta`)          | No       |

### Command (`command`)

Defines how to start the application. This is a list of strings, where the first element is usually the absolute path to the executable (relative to the container's interior), and subsequent elements are arguments passed to the executable.

```yaml
command:
  - /opt/apps/org.deepin.calculator/files/bin/deepin-calculator
  # - --some-argument # More arguments can be added
```

| Name    | Description                                                                 | Required |
| ------- | --------------------------------------------------------------------------- | -------- |
| command | Defines the executable path and its argument list for starting the application. Usually required for `kind: app`. | No       |

### Base Environment (`base`)

Specifies the minimum root filesystem required for building and running. This field is required.

```yaml
base: org.deepin.base/23.1.0
```

| Name    | Description                                      | Required |
| ------- | ------------------------------------------------ | -------- |
| base    | Identifier for the base, format `id/version`. Version number supports three-digit fuzzy matching. | Yes      |

### Runtime (`runtime`)

Application runtime dependencies, which are also build dependencies.

```yaml
runtime: org.deepin.runtime.dtk/23.1.0
```

| Name    | Description                                            |
| ------- | ------------------------------------------------------ |
| id      | Unique name of the runtime                             |
| version | Runtime version, three digits can fuzzy match the fourth |

### Sources (`sources`)

Describes the source information required for the project. `sources` is a list and can contain multiple source items. Fetched sources are stored by default in the `linglong/sources` directory at the same level as `linglong.yaml`.

#### Git Type

```yaml
sources:
  - kind: git
    url: https://github.com/linuxdeepin/deepin-calculator.git
    version: master # or tag
    commit: d7e207b4a71bbd97f7d818de5044228c1a6e2c92 # Optional, for specifying the exact commit
    name: deepin-calculator.git # Optional, specifies the directory name after download
```

| Name    | Description                                                                 | Required (within a single source) |
| ------- | --------------------------------------------------------------------------- | --------------------------------- |
| kind    | `git`, indicates download using the git tool.                               | Yes                               |
| url     | Source repository address                                                   | Yes                               |
| version | Branch or tag of the source repository                                      | No (defaults to the main branch)  |
| commit  | Hash value of a specific commit, used for precise checkout                  | No                                |
| name    | Optional, specifies the subdirectory name under `linglong/sources` after download. | No                                |

#### File Type

```yaml
sources:
  - kind: file
    url: https://example.com/some-file.dat
    digest: sha256:... # Recommended to provide sha256 hash
    name: my-data.dat # Optional, specifies the filename after download
```

| Name   | Description                                                                 | Required (within a single source) |
| ------ | --------------------------------------------------------------------------- | --------------------------------- |
| kind   | `file`, indicates direct file download.                                     | Yes                               |
| url    | File download address                                                       | Yes                               |
| digest | Optional, sha256 hash of the file, used for verification.                   | No                                |
| name   | Optional, specifies the filename under `linglong/sources` after download.   | No                                |

#### Archive Type

```yaml
sources:
  - kind: archive
    url: https://github.com/linuxdeepin/deepin-calculator/archive/refs/tags/6.5.4.tar.gz
    digest: 9675e27395891da9d9ee0a6094841410e344027fd81265ab75f83704174bb3a8 # Recommended to provide sha256 hash
    name: deepin-calculator-6.5.4 # Optional, specifies the directory name after extraction
```

| Name   | Description                                                                 | Required (within a single source) |
| ------ | --------------------------------------------------------------------------- | --------------------------------- |
| kind   | `archive`, downloads and automatically extracts the archive. Supports common compression formats. | Yes                               |
| url    | Archive download address                                                    | Yes                               |
| digest | Optional, sha256 hash of the archive file, used for verification.           | No                                |
| name   | Optional, specifies the directory name under `linglong/sources` after extraction. | No                                |

#### DSC Type

```yaml
sources:
  - kind: dsc
    url: https://cdn-community-packages.deepin.com/deepin/beige/pool/main/d/deepin-calculator/deepin-calculator_6.0.1.dsc
    digest: ce47ed04a427a887a52e3cc098534bba53188ee0f38f59713f4f176374ea2141 # Recommended to provide sha256 hash
    name: deepin-calculator-dsc # Optional, specifies the directory name after download and extraction
```

| Name   | Description                                                                 | Required (within a single source) |
| ------ | --------------------------------------------------------------------------- | --------------------------------- |
| kind   | `dsc`, handles Debian source package description files and associated files. | Yes                               |
| url    | `.dsc` file download address                                                | Yes                               |
| digest | Optional, sha256 hash of the `.dsc` file, used for verification.            | No                                |
| name   | Optional, specifies the directory name under `linglong/sources` after download and extraction. | No                                |

### Export Rules (`exclude`/`include`)

When exporting the built application to a UAB package, files to be trimmed are specified as shown in the example below:

```yaml
exclude:
  - /usr/share/locale # Trim the entire folder
  - /usr/lib/libavfs.a # Trim a single file

include:
  - /usr/share/locale/zh_CN.UTF-8 # Used with exclude to export only specific files under a folder
```

| Name    | Description                                                              |
| ------- | ------------------------------------------------------------------------ |
| exclude | Absolute path within the container, can be a file or folder, used for exclusion. |
| include | Absolute path within the container, files that must be included in the UAB package (even if the parent directory is excluded). |

### Build Rules (`build`)

Describes how to compile and install the project in the build environment. This field is required, and its content is a multi-line string treated as a shell script.

Describes the build rules.

```yaml
build: |
  qmake -makefile PREFIX=${PREFIX} LIB_INSTALL_DIR=${PREFIX}/lib/${TRIPLET}
  make
  make install
```

| Name  | Description                                                                 | Required |
| ----- | --------------------------------------------------------------------------- | -------- |
| build | Shell script executed during the build phase. Runs inside the container build environment. | Yes      |

### Build Extensions (`buildext`)

Optional field for defining extension behaviors for specific build stages. Currently mainly supports the `apt` extension for managing Debian package dependencies required for build and runtime.

```yaml
buildext:
  apt:
    build_depends: # Build-time dependencies, installed only in the build environment
      - build-essential
      - cmake
      - qt6-base-dev
    depends: # Runtime dependencies, included in the final application or runtime
      - libqt6widgets6
      - libglib2.0-0
```

| Name        | Description                                                               | Required |
| ----------- | ------------------------------------------------------------------------- | -------- |
| buildext    | Container block for build extension configurations.                       | No       |
| apt         | Extension configuration using the apt package manager.                    | No       |
| build_depends | A list of strings listing packages needed at build time, not included in the final artifact. | No       |
| depends     | A list of strings listing packages needed at runtime, included in the final artifact. | No       |

### Modules (`modules`)

Optional field for splitting files installed under the `${PREFIX}` directory into different modules. Useful for on-demand downloads or providing optional features.

```yaml
modules:
  - name: main # Main module name, usually same as or related to package.id
    files: # List of files or directories included in this module (relative to ${PREFIX})
      - bin/
      - share/applications/
      - share/icons/
      - lib/ # Includes all libraries
  - name: translations # Translation module
    files:
      - share/locale/
  - name: extra-data # Optional data module
    files:
      - share/my-app/optional-data/
```

| Name    | Description                                                                                                                                                           | Required |
| ------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------- | -------- |
| modules | List defining the rules for splitting the application into modules.                                                                                                   | No       |
| name    | Name of the module. Each module requires a unique name.                                                                                                               | Yes (within each module item) |
| files   | A list of strings listing files or directories belonging to this module. Paths are relative to `${PREFIX}`.                                                             | Yes (within each module item) |

**Note:** All files installed under `${PREFIX}` will be assigned to a module. When `modules` is not defined, the build system automatically generates default `binary` and `develop` modules.

### Variables

Describes variables that can be used during the build process.

| Name    | Description                                                                                                   |
| ------- | ------------------------------------------------------------------------------------------------------------- |
| PREFIX  | Environment variable used under the `build` field; provides the installation path during build, e.g., /opt/apps/org.deepin.calculator |
| TRIPLET | Environment variable used under the `build` field; provides a triplet containing architecture information, e.g., x86_64-linux-gnu           |

## Complete Example

### Build Application

#### Calculator

```yaml
version: '1'

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
    # Extra packages depended on during build
    build_depends: []
    # Extra packages depended on during runtime
    depends: []
```

### Build Root Filesystem

```bash
git clone git@github.com:linglongdev/org.deepin.foundation.git
cd org.deepin.foundation
bash build_base.sh beige amd64
```

This project is used to build the root filesystem used by Linglong. `beige` refers to the distribution codename, and `amd64` refers to the architecture.

| Distribution Version | Architecture              |
| -------------------- | ------------------------- |
| eagle (UOS 20)       | amd64, arm64, loongarch64 |
| beige (deepin 23)    | amd64, arm64              |

### Build Runtime

```bash
git clone git@github.com:linglongdev/org.deepin.Runtime.git -b v23
cd org.deepin.Runtime
./depend-deb-list.sh | ./tools/download_deb_depend.bash
ll-builder build --skip-fetch-source
```

Adds basic environments like Qt on top of the root filesystem.
