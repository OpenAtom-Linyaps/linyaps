# 基础概念介绍

本章从包类型、运行环境、存储与容器、系统架构四个方面介绍如意玲珑的工作原理。第一次构建应用时不必掌握全部细节；遇到 Base/Runtime 选型、驱动、依赖、容器或仓库问题时再按需查阅。

## 包类型与使用场景

如意玲珑使用 `kind` 区分包的职责。应用运行时，系统按需组合 Base、Runtime、App 和 Extension，而不是把所有内容安装到宿主机根文件系统。

| 类型 | 作用 | 典型使用场景 |
| --- | --- | --- |
| Base | 提供 glibc、Shell 等基础系统组件，是容器用户空间的基础 | 选择应用已经适配的基础环境 |
| Runtime | 在 Base 之上提供可复用的框架和库 | DTK/Qt、Qt WebEngine、Wine 等多个应用共同使用的运行环境 |
| App | 提供应用自身的程序、资源、元数据和启动命令 | 桌面应用、命令行应用或服务程序 |
| Extension | 在不修改 App、Runtime 或 Base 的情况下补充运行时内容 | 与宿主机硬件匹配的闭源显卡驱动等可选组件 |

### Base

Base 可以理解为轻量化的“最小系统镜像”。它使应用不必直接依赖宿主机发行版提供的系统库，是跨发行版运行的基础。Base 并不等于完整桌面系统，也不应承载每个应用特有的依赖。

### Runtime

Runtime 依赖于 Base，为多个应用提供可共享的框架库。应用应先选择合适的 Base，再选择与它兼容的 Runtime；如果应用只依赖 Base 已提供的组件，也可以不声明 Runtime。

可用版本和兼容关系见[运行时组件](./runtime.md)。

### App

App 是最终交付给用户的应用内容。构建脚本通常把可执行文件、库和资源安装到 `${PREFIX}`，运行时映射到 `/opt/apps/<应用 ID>/files`。应用通过 `command` 声明入口，并通过 desktop 文件、图标等资源集成到桌面环境。

### Extension

Extension 用来按需补充内容，避免把硬件相关或可选组件固化到所有应用中。运行应用时可由系统选择合适的扩展，也可以通过 `ll-cli run --extensions` 或 `ll-builder run --extensions`显式指定。

显卡驱动是典型场景：Mesa 通常由 Base 提供，闭源或厂商驱动需要与宿主机驱动匹配。具体示例见[驱动程序](./driver.md)。

## 容器与隔离

应用在 Rootless 容器中构建和运行。运行时会将 Base、可选 Runtime、App 和 Extension 组合成应用看到的文件系统，并使用 Linux Namespace 隔离进程、挂载点等资源。容器不是虚拟机：应用仍与宿主机共享内核，因此硬件驱动、图形会话、D-Bus 和文件访问需要通过受控方式与宿主机协作。

应用目录在运行时通常只读。配置、数据和缓存应分别写入 `XDG_CONFIG_HOME`、`XDG_DATA_HOME` 和 `XDG_CACHE_HOME`；桌面文件、图标等需要由系统识别的资源会从包中导出到宿主环境。常见路径问题见[常见问题](../tips-and-faq/faq.md)。

## OSTree 与仓库

如意玲珑使用 OSTree 存储和分发版本化内容。仓库中的 Base、Runtime、App 和 Extension 由内容寻址对象组成，不同版本可以复用未变化的数据。安装或升级时，客户端获取元数据并拉取所需对象，因此可以避免每次重复下载全部内容。

本地构建完成后，`ll-builder` 会把产物提交到本地构建缓存；发布时可以导出 UAB，或将内容推送到远程仓库。用户侧的包管理服务负责从仓库安装、升级和卸载内容。仓库配置和镜像机制见[仓库管理](../publishing/repositories.md)与[镜像站点](../publishing/mirrors.md)。

## 架构与工具链

```text
linglong.yaml / deb / AppImage / Flatpak
                 │
       ll-builder / 转换工具
                 │
        本地 OSTree 缓存 ──→ UAB 或远程仓库
                 │                       │
                 └──────── 包管理服务 ←─┘
                              │
                           ll-cli
                              │
                Base + Runtime + App + Extension
                              │
                         Rootless 容器
```

- **`ll-cli`** 是用户侧命令行前端，用于搜索、安装、运行、升级、卸载应用和管理仓库。它通过包管理服务完成需要持久化的包操作，并负责发起应用运行。
- **`ll-builder`** 是开发者工具，读取 `linglong.yaml`，准备 Base/Runtime 和源码，在隔离环境中构建、调试、验证、导出及推送应用。
- **包管理服务（Package Manager）** 管理本地仓库、远程仓库和已安装内容，为 `ll-cli` 提供安装、升级、卸载等能力。
- **`ll-pica`** 从 deb 生成构建配置并调用 `ll-builder` 完成转换；`ll-appimage-convert` 和 `ll-pica-flatpak` 分别处理 AppImage 和 Flatpak 来源。

工具的完整参数见[命令参考](./commands/ll-cli/ll-cli.md)。开发者可以从[构建第一个如意玲珑应用](../start/build_your_first_app.md)回到实际工作流。
