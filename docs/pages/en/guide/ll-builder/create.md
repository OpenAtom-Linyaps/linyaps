<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# Create Linglong project

Use `ll-builder create` to create a Linglong project.

View the help information for the `ll-builder create` command:

```bash
ll-builder create --help
```

Here is the output:

```text
Usage: ll-builder [options] create <org.deepin.demo>

Options:
  -v, --verbose show detail log
  -h, --help Displays help on commandline options.
  --help-all Displays help including Qt specific options.

Arguments:
  create create build template project
  name project name
```

The `ll-builder create` command creates a folder in the current directory according to the project name, and generates the `linglong.yaml` template file required for the build. Here is an example:

```bash
ll-builder create org.deepin.demo
```

Here is the output:

```text
org.deepin.demo/
└── linglong.yaml
```

## Edit linglong.yaml

### The syntax version of the linglong.yaml file.

```yaml
version: "1"
```

### App meta info

```yaml
package:
  id: org.deepin.demo
  name: deepin-demo
  version: 0.0.0.1
  kind: app
  description: |
    simple Qt demo.
```

### Base

The minimum root filesystem.

```yaml
base: org.deepin.foundation/23.0.0
```

### Runtime

On the basis of the rootfs, add fundamental environments such as Qt.

```yaml
runtime: org.deepin.Runtime/23.0.1
```

### Command

Linglong application startup command.

```yaml
command:
  - /opt/apps/org.deepin.demo/files/bin/demo
```

### Source

Use git source code

```yaml
source:
  kind: git
  url: "https://github.com/linuxdeepin/linglong-builder-demo.git"
  commit: a3b89c3aa34c1aff8d7f823f0f4a87d5da8d4dc0
```

### Build

The commands required to build the project.

```yaml
build: |
  cd /project/linglong/sources/linglong-builder-demo.git
  qmake demo.pro
  make -j${JOBS}
  make install
```

### Completed `linglong.yaml` config

The contents of the `linglong.yaml` file are as follows:

```yaml
version: "1"

package:
  id: org.deepin.demo
  name: deepin-demo
  version: 0.0.0.1
  kind: app
  description: |
    simple qt demo.

command:
  - /opt/apps/org.deepin.demo/files/bin/demo

base: org.deepin.foundation/23.0.0
runtime: org.deepin.Runtime/23.0.1

sources:
  - kind: git
    url: "https://github.com/linuxdeepin/linglong-builder-demo.git"
    version: master
    commit: a3b89c3aa34c1aff8d7f823f0f4a87d5da8d4dc0

build: |
  cd /project/linglong/sources/linglong-builder-demo.git
  qmake -makefile PREFIX=${PREFIX} LIB_INSTALL_DIR=${PREFIX}/lib/${TRIPLET}
  make
  make install
```
