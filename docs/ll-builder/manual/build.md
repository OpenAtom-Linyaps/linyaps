# ll-builder build
ll-builder提供build命令进行构建。参数如下：
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

build命令必须运行在工程的根目录，即linglong.yaml文件所在位置。以一个简单[qt demo](https://gitlabwh.uniontech.com/ut001198/org.deepin.demo.git)为例：

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

下载示例应用到本地，进入linglong.yaml文件目录后执行build命令将开始构建。

```bash
cd org.deepin.demo
```

```bash
ll-builder build
```

构建完成后，构建内容将自动提交到本地ostree缓存中。导出构建内容见ll-builder export。

使用exec参数可在构建脚本执行前进入玲珑容器：

```bash
ll-builder build --exec /bin/bash
```
进入容器后，可执行其他shell命令。