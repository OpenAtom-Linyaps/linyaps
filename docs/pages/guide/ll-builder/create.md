<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# 创建项目

`ll-builder create`命令用来创建玲珑项目。

查看 `ll-builder create`命令的帮助信息：

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

`ll-builder create`命令根据输入的项目名称在当前目录创建对应的文件夹，同时生成构建所需的 `linglong.yaml`模板文件。示例如下：

```bash
ll-builder create org.deepin.demo
```

`ll-builder create org.deepin.demo`命令输出如下：

```text
org.deepin.demo/
└── linglong.yaml
```

## 编辑 linglong.yaml

### linglong.yaml 文件语法的版本

```
version: "1"
```

### 软件包元信息配置

```yaml
package:
  id: org.deepin.demo
  name: hello
  version: 0.0.0.1
  kind: app
  description: |
    simple Qt demo.
```

### 基础环境

最小的根文件系统。

```yaml
base: org.deepin.foundation/23.0.0
```

### 运行时

在根文件系统基础上添加 Qt 等基础环境。

```yaml
runtime: org.deepin.Runtime/23.0.1
```

### 启动命令

玲珑应用的启动命令。

```yaml
command: [echo, -e, hello world]
```

### 源码

使用 git 源码

```yaml
sources:
  kind: git
  url: "https://github.com/linuxdeepin/linglong-builder-demo.git"
  version: master
  commit: a3b89c3aa34c1aff8d7f823f0f4a87d5da8d4dc0
```

### 构建

在容器内构建项目需要的命令。

```yaml
build: |
  cd /project/linglong/sources/linglong-builder-demo.git
  qmake demo.pro
  make -j${JOBS}
  make install
```

### 完整的 linglong.yaml 配置

`linglong.yaml`文件内容如下：

```yaml
version: "1"

package:
  id: @ID@
  name: your name #set your application name
  version: 0.0.0.1 #set your version
  kind: app
  description: |
    your description #set a brief text to introduce your application.

command: [echo, -e, hello world] #the commands that your application need to run.

base: org.deepin.foundation/23.0.0 #set the base environment, this can be changed.

#set the runtime environment if you need, a example of setting deepin runtime is as follows.
#runtime:
#org.deepin.Runtime/23.0.1

#set the source if you need, a simple example of git is as follows.
#sources:
#  - kind: git
#    url: https://github.com/linuxdeepin/linglong-builder-demo.git
#    version: master\n
#    commit: a3b89c3aa34c1aff8d7f823f0f4a87d5da8d4dc0

build: |
  echo 'hello' #some operation to build this project
```
