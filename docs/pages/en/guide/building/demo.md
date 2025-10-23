<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# Building Example Demonstration

## Initialize a Linyaps Application Project

```bash
ll-builder create org.deepin.demo
```

## Edit linglong.yaml Configuration File

### Configure Package Metadata Information

```yaml
package:
  id: org.deepin.demo
  name: demo
  kind: app
  version: 1.0.0.0
  description: |
    A simple demo app.
```

### Configure Application Launch Command

```yaml
command:
  - demo
```

### Configure Base System and Runtime Environment

```yaml
base: org.deepin.base/23.1.0
runtime: org.deepin.runtime.dtk/23.1.0
```

### Configure Source Code Information

Fetch source code using Git

```yaml
sources:
  - kind: git
    url: "https://github.com/linuxdeepin/linglong-builder-demo.git"
    commit: master
    name: linglong-builder-demo
```

### Configure Build Rules

```yaml
build: |
  cd /project/linglong/sources/linglong-builder-demo
  rm -rf build || true
  mkdir build
  cd build
  qmake ..
  make
  make install
```

### Complete linglong.yaml Configuration File

```yaml
version: "1"

package:
  id: org.deepin.demo
  name: demo
  kind: app
  version: 1.0.0.0
  description: |
    A simple demo app.

command:
  - demo

base: org.deepin.base/23.1.0
runtime: org.deepin.runtime.dtk/23.1.0

sources:
  - kind: git
    url: "https://github.com/linuxdeepin/linglong-builder-demo.git"
    commit: master
    name: linglong-builder-demo

build: |
  cd /project/linglong/sources/linglong-builder-demo
  rm -rf build || true
  mkdir build
  cd build
  qmake ..
  make
  make install
```

For more configuration file field definitions, refer to [Configuration File Reference](./manifests.md)

## Execute the Build Process

Execute the build command in the Linyaps project root directory:

```bash
ll-builder build
```

## Run the Application

After successful build, execute the run command in the Linyaps project directory to directly run the application without installation.

```bash
ll-builder run
```

## Export Build Artifacts

Execute the export command in the Linyaps project root directory to export the build contents.

```bash
ll-builder export --layer
```

The exported directory structure is as follows:

```text
├── linglong
├── linglong.yaml
├── org.deepin.demo_1.0.0.0_x86_64_binary.layer
└── org.deepin.demo_1.0.0.0_x86_64_develop.layer
```

## Additional Reference Examples

[qt5](https://github.com/linglongdev/cn.org.linyaps.demo.qt5) - qt5 program

[dtk5](https://github.com/linglongdev/cn.org.linyaps.demo.dtk5.qmake) - dtk5 + qmake

[dtk5](https://github.com/linglongdev/cn.org.linyaps.demo.dtk5.cmake) - dtk5 + cmake

[dtkdeclarative5](https://github.com/linglongdev/cn.org.linyaps.demo.dtkdeclarative5) - dtk5 + qml

[electron](https://github.com/myml/electron-vue-linyaps-app) - electron + vue example

[plantuml](https://github.com/linglongdev/com.plantuml.gpl) - a Java application for programmatic flowchart creation

[org.sumatrapdfreader](https://github.com/linglongdev/org.sumatrapdfreader) - a Wine application, PDF reader

## More Complete Example

[Complete Example](../start/build_your_first_app.md) - Contains a complete workflow example of how to build applications, export build contents, install, and run.
