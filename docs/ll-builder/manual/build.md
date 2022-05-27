# ll-builder build

`ll-builder build`命令用来构建玲珑应用。

查看`ll-builder build`命令的帮助信息：

```bash
ll-builder build --help
```

`ll-builder build`命令的帮助信息如下：

```plain
Usage: ll-builder [options] build

Options:
  -v, --verbose  show detail log
  -h, --help     Displays help on commandline options.
  --help-all     Displays help including Qt specific options.
  --exec <exec>  run exec than build script

Arguments:
  build          build project
```

build 命令必须运行在工程的根目录，即 linglong.yaml 文件所在位置。以一个简单[qt demo](https://gitlabwh.uniontech.com/ut001198/org.deepin.demo.git)为例：

该项目配置文件如下：

```yaml
package:
  kind: app
  id: org.deepin.demo
  name: deepin-demo
  version: 1.0.0
  description: |
    demo for deepin os.

base:
  id: org.deepin.Runtime
  version: 20.5.0

depends:
  - id: qtbase/5.11.3.15

source:
  kind: local

build:
  kind: qmake
```

以玲珑项目`org.deepin.demo`为例，构建玲珑应用主要步骤如下：

进入到`org.deepin.demo`项目工程目录：

```bash
cd org.deepin.demo
```

执行`ll-budler build`命令将开始构建玲珑应用:

```bash
ll-builder build
```

构建完成后，构建内容将自动提交到本地`ostree`缓存中。导出构建内容见 `ll-builder export`。

使用`--exec`参数可在构建脚本执行前进入玲珑容器：

```bash
ll-builder build --exec /bin/bash
```

进入容器后，可执行`shell`命令，如`gdb`、`strace` 等。

玲珑应用`debug`版本更多调试信息请参考：[FAQ](../reference/debug.md)
