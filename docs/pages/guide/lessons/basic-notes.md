# 玲珑应用构建工程基础知识
在正式开始构建一个玲珑应用工程前, 我们需要对关于玲珑应用构建的基础知识有一定的认知, 以此协助我们更好地决策我们在启动构建工程前决定要准备哪些材料、进行哪一类型的操作

## 玲珑应用构建基本步骤
在正式开始构建一个玲珑应用工程前, 我们需要了解一个玲珑应用从资源(源代码、二进制文件等)输入到应用安装包导出所经过的基本步骤, 来确定我们需要准备哪些必要文件

1. 获取构建目标源文件(开源项目源代码、应用二进制文件等)
2. 根据源文件判断玲珑应用构建类型, 选择合适的构建方案
3. 准备符合要求的玲珑构建环境
4. 按照构建类型及源代码内容定制构建配置文件 `linglong.yaml` 
5. 准备应用所使用的通用类资源, 图标以及其他非二进制资源

## 玲珑应用构建工程所需材料
结合上述的知识, 我们可以了解到一个玲珑应用在构建的全过程中, 主要涉及到以下的文件:

1. 玲珑应用构建工程配置文件 `linglong.yaml`
2. 应用源代码/需要封装的二进制文件等资源
3. 非二进制文件等通用资源

## 玲珑应用遵循的主流规范
每一款Linux桌面软件包管理方案为了能够保障完整的功能和良好的体验, 均需要遵守软件包管理方案提出的各类规范要求以最大限度发挥软件包管理方案的功能并保障应用生态体验.
如意玲珑也并不总是特立独行, 需要满足一定的规范来保障如意玲珑生态得以持续稳步发展.

目前如意玲珑生方案遵守以下主流的规范:

1. Freedesktop XDG规范
2. 玲珑应用目录结构规范
3. 玲珑应用构建工程配置文件 `linglong.yaml` 规范 

### Freedesktop XDG规范

1. 玲珑应用解决方案遵循Freedesktop XDG规范,一款正常的图形化应用应具备图标文件、desktop文件并符合Freedesktop XDG规范
2. 玲珑应用图标文件应该根据不同尺寸归类到`$PREFIX/share/icons/hicolor/目录下
3. 玲珑应用容器中使用 `XDG_DATA_DIRS` 等变量, 支持读写宿主机中的用户目录

### 玲珑应用目录结构规范

1. 玲珑应用遵循$PREFIX路径规则,该变量自动生成, 应用所有相关文件需存放于此目录下, 该目录层级下存在 `bin` `share` 等目录

2. 玲珑应用容器中的应用将不被允许读取宿主机中系统目录中的二进制文件、运行库

3. 在构建工程中, 构建工程目录将会被映射到玲珑容器中, 挂载为 `/project`

4. 玲珑应用容器中运行库、头文件所在目录将根据运行环境类型而异
foundation类: 在玲珑容器中映射为普通系统路径 `/usr/bin` `/usr/include` 等, 作为基础运行系统环境存在
runtime类: 在玲珑容器中映射为runtime容器路径 `/runtime/usr/bin` `/runtime/usr/include` 等, 作为基础运行系统环境存在

\*默认情况下, 玲珑容器内部的环境变量已自动处理好路径识别问题, 如:

```
PATH=szbt@szbt-linyaps23:/project$ echo $PATH
/bin:/usr/bin:/runtime/bin:/opt/apps/com.tencent.wechat/files/bin:/usr/local/bin:/usr/bin:/bin:/usr/local/games:/usr/games:/sbin:/usr/sbin
```

通用表达为:
```
PATH=szbt@szbt-linyaps23:/project$ echo $PATH
/bin:/usr/bin:/runtime/bin:$PREFIX/bin:/usr/local/bin:/usr/bin:/bin:/usr/local/games:/usr/games:/sbin:/usr/sbin
```

## 玲珑应用构建工程通用资源的规范
在玲珑应用构建工程中, 不同的资源文件均需要遵循相关规范以确保构建、体验能够满足要求

### icons 图标目录规范
依据玲珑遵循的 `Freedesktop XDG规范` 及 `玲珑应用目录结构规范`, 图标根据不同尺寸放置在对应的目录中

主流的非矢量图标尺寸有: `16x16` `24x24` `32x32` `48x48` `128x128` `256x256` `512x512`
为保障图标在系统中能够获得较佳的体验效果, 因此需要至少一个尺寸不小于 `128x128` 的非矢量图标文件, 矢量图标则不存在该限制

因此, 一款玲珑应用安装目录中, icons图标目录应为以下示例:

```
$PREFIX/share/icons/hicolor/16x16/apps
$PREFIX/share/icons/hicolor/24x24/apps
$PREFIX/share/icons/hicolor/32x32/apps
$PREFIX/share/icons/hicolor/48x48/apps
$PREFIX/share/icons/hicolor/128x128/apps
$PREFIX/share/icons/hicolor/256x256/apps
$PREFIX/share/icons/hicolor/512x512/apps
$PREFIX/share/icons/hicolor/scalable/apps
```
\* `scalable` 目录用于放置 `矢量图标` 文件, 一般为 `.svg` 格式

假设你的玲珑应用同时提供尺寸为 `128x128` 的非矢量图标文件 `linyaps-app-demo.png` 和 `128x128` 的矢量图标文件 `linyaps-app-demo.svg`, 在玲珑容器中应当表现为以下状态
```
$PREFIX/share/icons/hicolor/128x128/apps/linyaps-app-demo.png
$PREFIX/share/icons/hicolor/scalable/apps/linyaps-app-demo.svg
```
\* 为了避免图标冲突被覆盖, 图标文件名请使用应用 `唯一英文名称` 或 `玲珑应用id`

### desktop 文件规范
玲珑应用兼容大部分符合 `Freedesktop XDG规范` 的 `desktop启动文件`, 其中有以下字段需要额外注意:

| 字段 | 值要求 |
|-------|---------|
| Exec | 该值用于设置点击此desktop文件时执行的指令, 需要与 `linglong.yaml` 中的 `command` 值保持一致 |
| Icon | 该值用于设置该desktop文件显示的应用图标, 需要与 `icons 图标目录规范` 中的图标文件名一致, 此值不需要文件名后缀 |

因此, 一个符合玲珑应用规范的desktop文件可以参考:

```
org.qbittorrent.qBittorrent.desktop
[Desktop Entry]
Categories=Network;FileTransfer;P2P;Qt;
Exec=/opt/apps/org.qbittorrent.qBittorrent/files/bin/qbittorrent %U
Comment=Download and share files over BitTorrent
Icon=qbittorrent
MimeType=application/x-bittorrent;x-scheme-handler/magnet;
Name=qBittorrent
Type=Application
StartupWMClass=qbittorrent
Keywords=bittorrent;torrent;magnet;download;p2p;

StartupNotify=true
Terminal=false
```

## 玲珑应用构建工程 `linglong.yaml` 规范
正如其他传统包管理套件一样, 手动创建一个玲珑应用构建工程需要设置构建规则文件 `linglong.yaml`, 在构建规则中, 则根据用途划分为 `全局字段` 及 `定制化字段`.
\* 案例中 `linglong.yaml` 正文内所有空格符号、占位符均为有效字符, 请勿删除或变更格式

### 全局字段规范
在 `linglong.yaml` 中, 对于不受构建类型影响的字段我们称为 `全局字段`, 主要有以下参考的规范:

1. 一个可以正常开始构建工程的 `linglong.yaml` 应包含以下的关键部分:
| 模块 | 解释 |
|-------|-------|
| version | 构建工程版本号 |
| package | 玲珑应用基本信息 |
| base | 玲珑应用容器基础环境及版本设置, 基础环境中包含了部分基础运行库 |
| runtime | 玲珑应用运行库 `runtime` 及版本设置, 当 `base` 中的基础运行库满足程序运行要求时, 此模块可删除 |
| command | 玲珑应用容器启动时执行的命令, 与 `desktop` 文件的 `Exec` 字段内容一致 |
| sources | 玲珑应用构建工程源文件类型 |
| build | 玲珑应用构建工程将要执行的构建规则 |

`package` 模块中存在数个子模块:
| 子模块 | 解释 |
|-------|-------|
| id | 玲珑应用id/包名 |
| name | 玲珑应用名称, 使用英文名称 |
| version | 玲珑应用版本号 |
| kind | 玲珑应用类型, 默认为 `app` |
| description | 玲珑应用描述 |

2. 玲珑应用遵循 `$PREFIX` 路径规则,该变量自动生成，应用所有相关文件需存放于此目录下. 构建规则中若有需要涉及安装文件的操作, 均需要安装到 `$PREFIX` 路径下
\* `$PREFIX` 变量名即为填写的实际内容, **请勿使用 `绝对路径` 或任何具有绝对值作用的内容代替 **

3. 玲珑应用目前遵循 `四位数字` 的版本号命名规则,不符合规则无法启动构建工程

4. `base`、`runtime` 版本支持自动匹配最新版本 `尾号`,版本号可以仅填写版本号的`前三位数字`.如:
当base `org.deepin.foundation`同时提供 `23.0.0.28` `23.0.0.29`, 若 `linglong.yaml` 中仅填写
```
base: org.deepin.foundation/23.0.0
```
那么在启动玲珑应用构建工程时, 将会默认采用最高版本号的 `23.0.0.29`

5 玲珑应用构建工程配置文件目前不直接兼容其他包构建工具的配置文件,需要根据构建工程配置文件案例来进行适配修改:
https://linglong.dev/guide/ll-builder/manifests.html


### 定制化字段
根据玲珑应用构建工程源文件类型, 又可将玲珑应用构建工程划分为 `本地文件文件构建` `git 源码仓库拉取构建`, 不同类型则需要填写不同的 `linglong.yaml` 
玲珑应用构建工程源文件类型 `sources` 主要支持这几种类型: `git` `local` `file` `archive`
完整说明参考: [构建配置文件简介](https://linyaps.org.cn/guide/ll-builder/manifests.html)

#### git拉取源码编译模式
当玲珑应用构建工程需要通过git拉取开源项目仓库资源到本地进行构建时, 此时 `sources` 应当设置为 `git` 类型, 并根据要求填写 `linglong.yaml` 
此时需要根据规范编写 `sources` 与 `build` 模块

1. `sources` 示例:
```yaml
sources:
  - kind: git
    url: https://githubfast.com/qbittorrent/qBittorrent.git
    version: release-4.6.7
    commit: 839bc696d066aca34ebd994ee1673c4b2d5afd7b

  - kind: git
    url: https://githubfast.com/arvidn/libtorrent.git
    version: v2.0.9
    commit: 4b4003d0fdc09a257a0841ad965b22533ed87a0d
```

| 名称 | 描述 |
|------|-------|
| kind | 源文件类型 |
| url | 需要通过git拉取的源代码仓库地址, 该仓库需要支持git功能. 当网络状态不佳时, 可采用镜像地址代替 |
| version | 指定源代码仓库的版本号, 即 `tag标签`, 或拉取主线 `master` |
| commit | 根据该仓库 `commit` 变动历史拉取源码, 此处填入commit对应的值, 将会应用该仓库截止本commit的所有变更. *此字段优先级高于 `version`, 请勿填入 `version` 合并时间之后的任何 `commit`|
\* 支持同时添加多个git仓库作为 `sources` 拉取

2. `build` 示例:
```yaml
build: |
  mkdir -p ${PREFIX}/bin/ ${PREFIX}/share/
  ##Apply patch for qBittorrent
  cd /project/linglong/sources/qBittorrent.git
  git apply -v /project/patches/linyaps-qBittorrent-4.6.7-szbt2.patch
```
此模块为构建规则正文, 路径遵守 `玲珑应用目录结构规范`
在 `sources` 拉取到本地后, 仓库文件将会存放在 `/project/linglong/sources` 目录中, 此时不同仓库目录以 `xxx.git` 命名
支持运用 `git patch` 功能对源代码进行便捷维护

#### 本地资源操作模式
当玲珑应用构建工程需要对构建目录中的文件操作时, 此时 `kind` 应当设置为 `local` 类型, 并根据要求填写 `linglong.yaml` 
此时需要根据规范编写 `sources` 与 `build` 模块

1. `sources` 示例:
```yaml
sources:
source:
  - kind: local
    name: "qBittorrent"
```

| 名称 | 描述 |
|------|-------|
| kind | 源文件类型 |
| name | 源文件名称标识, 不具备实际用途 |

\* 当 `kind` 应当设置为 `local` 类型时, 构建工程将不会对任何源文件进行操作

2. `build` 示例:
```yaml
build: |
  ##Build main
  mkdir /project/src/qBittorrent-release-4.6.7-szbt2/build
  cd /project/src/qBittorrent-release-4.6.7-szbt2/build
  cmake -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=$PREFIX ..
  make -j$(nproc)
  make install
```
此模块为构建规则正文, 路径遵守 `玲珑应用目录结构规范`
此时 `build` 规则支持多种写法以模拟人为操作
\* 需要确保此构建规则所有步骤均可以正常被执行, 否则将会中断当次构建任务

#### 容器内部手动操作模式
若计划直接进入玲珑容器手动操作而不是通过构建规则文件 `linglong.yaml`,那么应该参考 `本地资源操作模式` 填写 `linglong.yaml`

1. `sources` 部分写法与 `本地资源操作模式` 一致
2. 由于使用手动操作, 因此不需要完整且可以正常被执行的 `build` 规则, 此时 `linglong.yaml` 用于生成符合描述的玲珑容器而不是执行所有任务, 具体操作可查阅后续课程关于容器内部构建文件的案例