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
ll-builder create <org.deepin.demo>
```

Here is the output:

```text
org.deepin.demo/
└── linglong.yaml
```

## Edit linglong.yaml

### App meta info

```yaml
package:
  id: org.deepin.demo
  name: deepin-demo
  version: 0.0.1
  kind: app
  description: |
    simple Qt demo.
```

### runtime

```yaml
runtime:
  id: org.deepin.Runtime
  version: 23.0.0
```

### Dependencies

```yaml
depends:
  - id: icu
    version: 63.1.0
    type: runtime
```

### Source

Use git source code

```yaml
source:
  kind: git
  url: "https://github.com/linuxdeepin/linglong-builder-demo.git"
  commit: 24f78c8463d87ba12b0ac393ec56218240315a9
```

### Build template

The source code is a qmake project, and the build type is qmake (see qmake.yaml for the template content).

```yaml
build:
  kind: qmake
```

### Completed `linglong.yaml` config

The contents of the `linglong.yaml` file are as follows:

```yaml
package:
  id: org.deepin.demo
  name: deepin-demo
  version: 0.0.1
  kind: app
  description: |
    simple Qt demo.

runtime:
  id: org.deepin.Runtime
  version: 23.0.0

source:
  kind: git
  url: "https://github.com/linuxdeepin/linglong-builder-demo.git"
  commit: a3b89c3aa34c1aff8d7f823f0f4a87d5da8d4dc0

build:
  kind: qmake
```
