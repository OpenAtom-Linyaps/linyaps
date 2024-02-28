<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# 创建项目

`ll-builder create`命令用来创建玲珑项目。

查看`ll-builder create`命令的帮助信息：

```bash
ll-builder create --help
```

`ll-builder create`命令的帮助信息如下：

```text
Usage: ll-builder [options] create <org.deepin.demo>

Options:
  -v, --verbose  show detail log
  -h, --help     Displays help on commandline options.
  --help-all     Displays help including Qt specific options.

Arguments:
  create         create build template project
  name           project name
```

`ll-builder create`命令根据输入的项目名称在当前目录创建对应的文件夹，同时生成构建所需的`linglong.yaml`模板文件。示例如下：

```bash
ll-builder create <org.deepin.demo>
```

`ll-builder create org.deepin.demo`命令输出如下：

```text
org.deepin.demo/
└── linglong.yaml
```

## 编辑linglong.yaml

### 软件包元信息配置

```yaml
package:
  id: org.deepin.demo
  name: deepin-demo
  version: 0.0.1
  kind: app
  description: |
    simple Qt demo.
```

### 运行时

```yaml
runtime:
  id: org.deepin.Runtime
  version: 23.0.0
```

### 依赖项

```yaml
depends:
  - id: icu
    version: 63.1.0
    type: runtime
```

### 源码

使用git源码

```yaml
source:
  kind: git
  url: "https://github.com/linuxdeepin/linglong-builder-demo.git"
  commit: 24f78c8463d87ba12b0ac393ec56218240315a9
```

### 选择构建模板

源码为qmake工程，填写build 类型为qmake（模板内容见qmake.yaml）。

```yaml
build:
  kind: qmake
```

### 完整的linglong.yaml配置

`linglong.yaml`文件内容如下：

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

depends:
  - id: icu
    version: 63.1.0
    type: runtime

source:
  kind: git
  url: "https://github.com/linuxdeepin/linglong-builder-demo.git"
  commit: a3b89c3aa34c1aff8d7f823f0f4a87d5da8d4dc0

build:
  kind: qmake
```
