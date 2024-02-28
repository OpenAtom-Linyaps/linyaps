<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

## Initialize Linglong Projects

```text
ll-builder create org.deepin.calculator
```

## Edit linglong.yaml

### Fill in the meta information of packages

```text
package:
  id: org.deepin.calculator
  name: deepin-calculator
  version: 5.9.17
  kind: app
  description: |
    calculator for deepin os.
```
### Fill in the runtime info

```text
runtime:
  id: org.deepin.Runtime
  version: 23.0.0
```
### Fill in the source code info

Use git source code

```text
source:
  kind: git
  url: "https://github.com/linuxdeepin/deepin-calculator.git"
  commit: 7b5fdf8d133c356317636bb4b4a76fc73ef288c6
```
### Fill in the dependencies

```text
depends:
  - id: "dde-qt-dbus-factory"
    version: 5.5.12
  - id: googletest
    version: 1.8.1
  - id: icu
    version: 63.1.0
    type: runtime
  - id: xcb-util
    type: runtime
```
### Choose build template

The source code is a cmake project, and choose the build type as cmake (see cmake.yaml for the template content).

```text
build:
  kind: cmake
```

### Override template content

If the general template content does meet the build requirements, you can override the specified content in the linglong.yaml file. Variables or commands that are not re-declared in linglong.yaml will continue to be used.

Override the variable extra_args:

```text
variables:
  extra_args: |
   -DVERSION=1.1.1 \
   -DPREFIX=/usr
```

Override the build command “build”:
```text
build:
  kind: cmake
  manual :
    build: |
      cd ${build_dir} && make -j8
 
```
### Complete linglong.yaml

```text
package:
  id: org.deepin.calculator
  name: deepin-calculator
  version: 5.7.21
  kind: app
  description: |
    calculator for deepin os
variables:
  extra_args: |
    -DVERSION=${VERSION} \
    -DAPP_VERSION=5.7.21
runtime:
  id: org.deepin.Runtime
  version: 23.0.0
depends:
  - id: "dde-qt-dbus-factory"
    version: 5.5.12
  - id: googletest
    version: 1.8.1
  - id: icu
    version: 63.1.0
    type: runtime
  - id: xcb-util
    type: runtime
source:
  kind: git
  url: https://github.com/linuxdeepin/deepin-calculator.git
  commit: 7b5fdf8d133c356317636bb4b4a76fc73ef288c6
build:
  kind: cmake
```
## Start building

Execute the build subcommand in the root directory of Linglong projects:

```text
ll-builder build
```
## Export build content

Execute the export subcommand in the root directory of Linglong projects to check out the build content and generate the bundle package.

```text
ll-builder export
```
## Push to repositories

```text
ll-builder push org.deepin.calculator_5.7.21_x86_64.uab
```