<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# Build Example Demo

## Initialize a Linglong Application Project

```bash
ll-builder create org.deepin.demo
```

## Edit the linglong.yaml Configuration File

### Configure Package Metadata

```yaml
package:
  id: org.deepin.demo
  name: demo
  kind: app
  version: 1.0.0.0
  description: |
    A simple demo app.
```

### Configure Application Startup Command

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

Use Git to Fetch the Source Code

```yaml
sources:
  - kind: git
    url: "https://github.com/linuxdeepin/linglong-builder-demo.git"
    commit: master
    name: linglong-builder-demo
```

### Configure Build Rules

```yaml
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

For more details on configuration file fields, please refer to [Configuration File Description](https://www.google.com/search?q=./manifests.md)

## Execute the Build Process

In the root directory of the Linglong project, execute the build command:

```bash
ll-builder build
```

## Run the Application

After a successful build, execute the run command in the Linglong project directory. The application can be run directly without installation.

```bash
ll-builder run
```

## Export Build Artifacts

In the root directory of the Linglong project, execute the export command to check out the build content.

```bash
ll-builder export --layer
```

The directory structure after export is as follows:

```text
├── linglong
├── linglong.yaml
├── org.deepin.demo_1.0.0.0_x86_64_binary.layer
└── org.deepin.demo_1.0.0.0_x86_64_develop.layer
```

## More Reference Examples

[qt5](https://github.com/linglongdev/cn.org.linyaps.demo.qt5) - qt5 application

[dtk5](https://github.com/linglongdev/cn.org.linyaps.demo.dtk5.qmake) - dtk5 + qmake

[dtk5](https://github.com/linglongdev/cn.org.linyaps.demo.dtk5.cmake) - dtk5 + cmake

[dtkdeclarative5](https://github.com/linglongdev/cn.org.linyaps.demo.dtkdeclarative5) - dtk5 + qml

[electron](https://github.com/myml/electron-vue-linyaps-app) - electron + vue example

[plantuml](https://github.com/linglongdev/com.plantuml.gpl) - a Java application for drawing flowcharts programmatically

[org.sumatrapdfreader](https://github.com/linglongdev/org.sumatrapdfreader) - a Wine application, a PDF reader

## More Complete Examples

[Complete Example](https://www.google.com/search?q=../start/how_to_use.md) - a complete example that includes how to build an application, export build content, install, and run.
