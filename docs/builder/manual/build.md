# ll-builder build

```bash
Usage: ll-builder [options] build

Options:
  -v, --verbose  show detail log
  -h, --help     Displays help on commandline options.
  --help-all     Displays help including Qt specific options.
  --exec <exec>  run exec than build script

Arguments:
  build          build project
```
ll-builder build构建需要构建环境org.deepin.base。ll-builder build是玲珑应用的构建步骤，根据应用工程中的配置文件进行构建并将构建完成的应用导入仓库中。
**ll-builder build必须运行在应用工程目录的最上层且必须含有linglong.yaml配置文件。**

以reader应用工程目录为例

```bash
.
├── linglong.yaml
└── patches
    └── fix-install-prefix.patch
```
基础的应用工程目录必须包含构建配置文件linglong.yaml和可选的补丁目录patches(没有补丁可以不创建patches目录)。
linglong.yaml配置文件中含有应用信息，应用依赖，构建步骤等信息：

应用构建配置文件

```yaml
package:
  id: org.deepin.reader                                  ##应用appid
  version: 5.9.17                                        ##应用版本
  kind: app                                              ##构建类型
  description: |                                         ##应用简介
    reader for deepin os.
runtime:                                                 ##应用依赖runtime信息
  id: org.deepin.runtime                                 ##runtimeid
  version: 20.0.0                                        ##runtime版本
depends:                                                 ##应用依赖信息
  - id: libopenjp2                                       ##依赖应用id
    version: 2.4.0                                       ##依赖应用版本
  - id: djvu
    version: 3.5.28
source:                                                  ##源码信息
  kind: git                                              ##源码存放种类
  url: "http://xxxxxxxxxx.com/deepin-reader"             ##源码地址
  version: 5.9.9.2                                       ##应用仓库分支(git)
  commit: a42702f164d85525f09211381c77d2085c9c1057       ##应用源码sha值
  patch: patches/fix-install-prefix.patch                ##应用补丁路径与补丁名
build:                                                   ##应用构建信息
  kind: manual                                           ##应用构建方法
  manual: 
    configure: |                                         ##应用构建脚本
      echo "/runtime/lib" >> /etc/ld.so.conf.d/runtime.conf
      ldconfig
      mkdir build
      cd build
      qmake -r PREFIX=${PREFIX} ..
      make -j
      make test
      make -j install
      echo "LD_LIBRARY_PATH=/opt/apps/org.deepin.reader/files/lib/deepin-reader:${LD_LIBRARY_PATH}" > /opt/apps/org.deepin.reader/files/deepin-reader.sh
      echo "/opt/apps/org.deepin.reader/files/bin/deepin-reader" >> /opt/apps/org.deepin.reader/files/deepin-reader.sh
      chmod +x /opt/apps/org.deepin.reader/files/deepin-reader.sh
```
runtime构建配置文件
```yaml
package:
  id: org.deepin.runtime
  kind: runtime
  version: 20.0.0
  description: |
    runtime of deepin

base:
  id: org.deepin.base/20.0.0

depends:
  - id: qtbase/5.11.3.15
  - id: qttool/5.11.3
  - id: qtx11extras/5.11.3
  - id: qtsvg/5.11.3
  - id: qtxdg/3.5.0
  - id: qtmultimedia/5.11.3
  - id: "gsettings-qt/0.2"
  - id: dtkcommon/5.5.21
  - id: dtkcore/5.5.27
  - id: dtkgui/5.5.22
  - id: dtkwidget/5.5.40
  - id: "qt5platform-plugins/5.0.46"
  - id: qt5integration/5.5.19

build:
  kind: manual
  manual:
    configure: |
      echo skip configure
```
ll-builder build流程:
1.解析配置文件，读取应用信息，判断构建类型。

2.检查并导出所需runtime以及依赖应用(无依赖跳过)。

3.拉取应用源码，对比ref值验证源码完整性。

4.运行构建命令进行应用构建，导入仓库由ll-builder push导出uab包。

ll-builder --exec={path} build：该参数可以在应用构建之前运行命令，此功能用于调试构建，不要在生产环境中使用此参数。
