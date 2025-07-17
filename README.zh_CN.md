<!-- markdownlint-disable-next-line MD033 -->
# <img src="./misc/image/logo.svg" alt="如意玲珑图标" width="24" height="24" style="vertical-align:middle;">如意玲珑：更先进的 Linux 跨发行版软件包管理工具集

## :package: 介绍

[English](README.md) | [简体中文](README.zh_CN.md)

[![Contributors](https://img.shields.io/github/contributors/OpenAtom-Linyaps/linyaps)](https://github.com/OpenAtom-Linyaps/linyaps/graphs/contributors)
[![Latest Release](https://img.shields.io/github/v/release/OpenAtom-Linyaps/linyaps?style=flat&color=brightgreen)](https://github.com/OpenAtom-Linyaps/linyaps/releases)
[![Powered by Linyaps](https://img.shields.io/badge/powered%20by-Linyaps-ff69b4)](https://github.com/OpenAtom-Linyaps/linyaps)
[![Build Status](https://build.deepin.com/projects/linglong:CI:latest/packages/linyaps/badge.svg?type=default)](https://build.deepin.com/projects/linglong:CI:latest)
[![DeepSource](https://app.deepsource.com/gh/OpenAtom-Linyaps/linyaps.svg/?label=active+issues&show_trend=true&token=REPLACE_WITH_TOKEN)](https://app.deepsource.com/gh/OpenAtom-Linyaps/linyaps/)

[![GitHub Stars](https://img.shields.io/github/stars/OpenAtom-Linyaps/linyaps?style=social)](https://github.com/OpenAtom-Linyaps/linyaps/stargazers)
[![GitHub Forks](https://img.shields.io/github/forks/OpenAtom-Linyaps/linyaps?style=social&label=Fork)](https://github.com/OpenAtom-Linyaps/linyaps/network/members)
[![Code Size](https://img.shields.io/github/languages/code-size/OpenAtom-Linyaps/linyaps)](https://github.com/OpenAtom-Linyaps/linyaps)
[![GitHub Issues](https://img.shields.io/github/issues/OpenAtom-Linyaps/linyaps?style=social)](https://github.com/OpenAtom-Linyaps/linyaps/issues)

**如意玲珑**（Linyaps Is Not Yet Another Packaging System）是由如意玲珑社区团队开发并开源共建的**Linux 跨发行版软件包格式**，项目以独立沙盒容器的形式实现应用包的开发、管理、分发，用于替代 deb、rpm 等传统包管理工具，让 Linux 软件运行更兼容、更安全、更高效。

### :sparkles: 亮点

- **独创的非全量运行时（Runtime）设计**：基于标准化沙箱 Runtime，应用一次构建即可覆盖所有 Linux 发行版。Runtime 多版本共存且文件共享减少冗余，启动时通过动态库共享复用已加载资源，**速度提升显著，避免依赖冲突**。
- **非特权沙箱与双层隔离**：默认无 root 权限运行，通过内核  Namespace 隔离（进程/文件系统/网络等）构建**安全沙箱**。通过 OSTree 仓库提供原子化增量更新与版本回滚，相比全量沙箱方案，**资源占用更低**。

### :flags: 进展

- **发行版支持**：deepin、UOS、openEuler、Ubuntu、Debian、openKylin、Anolis OS，更多发行版适配中，欢迎参与贡献。
- **CPU 架构支持**：X86、ARM64、LoongArch，未来将提供对 RISC-V 等更多架构的支持。

## :gear: 安装

各个发行版安装方式如下：

### deepin 23

安装：

```sh
echo "deb [trusted=yes] https://ci.deepin.com/repo/obs/linglong:/CI:/release/Deepin_23/ ./" | sudo tee /etc/apt/sources.list.d/linglong.list
sudo apt update
sudo apt install linglong-bin
```

### Fedora 41

```sh
sudo dnf config-manager addrepo --from-repofile "https://ci.deepin.com/repo/obs/linglong:/CI:/release/Fedora_41/linglong%3ACI%3Arelease.repo"
sudo dnf update
sudo dnf install linglong-bin
```

### Ubuntu 24.04

```sh
echo "deb [trusted=yes] https://ci.deepin.com/repo/obs/linglong:/CI:/release/xUbuntu_24.04/ ./" | sudo tee /etc/apt/sources.list.d/linglong.list
sudo apt update
sudo apt install linglong-bin
```

### Debian 12

```sh
echo "deb [trusted=yes] https://ci.deepin.com/repo/obs/linglong:/CI:/release/Debian_12/ ./" | sudo tee /etc/apt/sources.list.d/linglong.list
sudo apt update
sudo apt install linglong-bin
```

### openEuler 23.09

```sh
sudo dnf config-manager --add-repo "https://ci.deepin.com/repo/obs/linglong:/CI:/release/openEuler_23.09/linglong%3ACI%3Arelease.repo"
sudo sh -c "echo gpgcheck=0 >> /etc/yum.repos.d/linglong%3ACI%3Arelease.repo"
sudo dnf update
sudo dnf install linglong-bin
```

### uos 1070

```sh
echo "deb [trusted=yes] https://ci.deepin.com/repo/obs/linglong:/CI:/release/uos_1070/ ./" | sudo tee /etc/apt/sources.list.d/linglong.list
sudo apt update
sudo apt install linglong-bin
```

### AnolisOS 8

```sh
sudo dnf config-manager addrepo --from-repofile "https://ci.deepin.com/repo/obs/linglong:/CI:/release/AnolisOS_8/linglong%3ACI%3Arelease.repo"
sudo dnf update
sudo dnf install linglong-bin
```

### openkylin 2.0

```sh
echo "deb [trusted=yes] https://ci.deepin.com/repo/obs/linglong:/CI:/release/openkylin_2.0/ ./" | sudo tee /etc/apt/sources.list.d/linglong.list
sudo apt update
sudo apt install linglong-bin
```

## :rocket: 快速开始

```sh
ll-cli install cn.org.linyaps.demo
ll-cli run cn.org.linyaps.demo
```

## :dart: 动机

在已经有 Snap、Flatpak、AppImage 这些已有项目情况下，我们开发如意玲珑包管理系统有以下几个原因：

- 2017 年，我们对 Flatpak 格式进行了研究，完成了 100+ 的软件包构建工作，后因其应用体积较大，磁盘占用过多、安全问题修复缓慢等各种原因没有继续适配。
- Snap 在除 Ubuntu 系统环境外有诸多兼容性问题，生态也不开放，因此也无法使用。
- AppImage 有着不错的可移植性，这些应用可以很轻松地在其他发行版上使用。但它没有集中的仓库存储和软件包管理功能，默认也不提供 Snap、Flatpak 同一级别的沙箱，安全性无法保障。

在经历过种种“折腾”后，基于对各类包管理器的了解，我们决定自己设计一套软件包管理系统。相比其他同类软件包格式，如意玲珑在启动速度、资源占用、安全性方面有诸多优势：

- 使用非全量运行时（Runtime），整体体积较小；由于复用同一运行时，应用启动速度会更快；
- 支持 rootless（非特权）沙盒。

由于在运行时和沙箱上的优化，玲珑有着比较显著的启动时间优势：

| 测试次数 | linyaps 间隔帧数 | linyaps 启动耗时（ms） | Flatpak 间隔帧数 | Flatpak 启动耗时（ms） | AppImage 间隔帧数 | AppImage 启动耗时（ms） | Snap 间隔帧数 | Snap 启动耗时（ms） |
| -------- | ----------------- | ----------------------- | ---------------- | ---------------------- | ----------------- | ----------------------- | ------------- | ------------------- |
| 1        | 9                 | 149.4                   | 14               | 232.4                  | 16                | 265.6                   | 42            | 697.2               |
| 2        | 9                 | 149.4                   | 13               | 215.8                  | 17                | 282.2                   | 41            | 680.6               |
| 3        | 8                 | 132.8                   | 9                | 149.4                  | 15                | 249                     | 40            | 664                 |
| 4        | 9                 | 149.4                   | 13               | 215.8                  | 15                | 249                     | 41            | 680.6               |
| 5        | 8                 | 132.8                   | 14               | 232.4                  | 16                | 265.6                   | 42            | 697.2               |
| 6        | 8                 | 132.8                   | 13               | 215.8                  | 15                | 249                     | 39            | 664                 |
| 7        | 9                 | 149.4                   | 12               | 199.2                  | 15                | 249                     | 39            | 647.4               |
| 8        | 8                 | 132.8                   | 14               | 232.4                  | 16                | 265.6                   | 40            | 680.6               |
| 平均     | 8.5               | 141.1                   | 12.8             | 213.7                  | 15.6              | 261.6                   | 40.5          | 676.2               |

## :incoming_envelope: 获取帮助

任何使用问题都可以通过以下方式寻求帮助。

- [Github Issues](https://github.com/OpenAtom-Linyaps/linyaps/issues)
- [论坛](https://bbs.deepin.org/module/detail/230)
- [联系我们](https://linyaps.org.cn/contactus)

## :memo: 文档导航

### 命令行工具

- [introduction](./docs/pages/en/guide/ll-cli/introduction.md)
- [install](./docs/pages/en/guide/ll-cli/install.md)
- [run](./docs/pages/en/guide/ll-cli/run.md)
- [uninstall](./docs/pages/en/guide/ll-cli/uninstall.md)
- [upgrade](./docs/pages/en/guide/ll-cli/update.md)
- [list](./docs/pages/en/guide/ll-cli/list.md)
- [prune](./docs/pages/en/guide/ll-cli/prune.md)
- [exec](./docs/pages/en/guide/ll-cli/exec.md)
- [content](./docs/pages/en/guide/ll-cli/content.md)
- [info](./docs/pages/en/guide/ll-cli/info.md)
- [ps](./docs/pages/en/guide/ll-cli/ps.md)
- [kill](./docs/pages/en/guide/ll-cli/kill.md)
- [search](./docs/pages/en/guide/ll-cli/query.md)

### 构建工具

- [introduction](./docs/pages/en/guide/ll-builder/introduction.md)
- [demo](./docs/pages/en/guide/ll-builder/demo.md)
- [create](./docs/pages/en/guide/ll-builder/create.md)
- [run](./docs/pages/en/guide/ll-builder/run.md)
- [push](./docs/pages/en/guide/ll-builder/push.md)
- [export](./docs/pages/en/guide/ll-builder/export.md)

### 包转换工具

#### deb 包转换

- [introduction](./docs/pages/en/guide/ll-pica/introduction.md)
- [init](./docs/pages/en/guide/ll-pica/init.md)
- [convert](./docs/pages/en/guide/ll-pica/convert.md)
- [adep](./docs/pages/en/guide/ll-pica/adep.md)

#### AppImage 包转换

- [introduction](./docs/pages/en/guide/ll-appimage-convert/introduction.md)
- [convert](./docs/pages/en/guide/ll-appimage-convert/convert-appimage.md)

#### Flatpak 包转换

- [introduction](./docs/pages/en/guide/ll-flatpak-convert/introduction.md)
- [convert](./docs/pages/en/guide/ll-flatpak-convert/convert-flatpak.md)

### 调试

- [debug](/docs/pages/en/guide/debug/debug.md)

### 常见问题

- [运行相关常见问题](/docs/pages/en/guide/debug/faq.md)
- [如意玲珑构建工具常见问题](/docs/pages/en/guide/debug/ll-builder-faq.md)
- [如意玲珑转换工具常见问题](/docs/pages/en/guide/debug/ll-pica-faq.md)

## :book: 学习和参考

### 相关项目

- [OStree](https://github.com/ostreedev/ostree)
- [如意玲珑打包工具 - ll-killer-go](https://github.com/System233/ll-killer-go)
- [如意玲珑网页商店](https://github.com/yoloke/Linglong-Shop)

### 系列教程

- [玲珑应用构建工程基础知识](/docs/pages/guide/lessons/basic-notes.md)
- [容器内手动编译应用](/docs/pages/guide/lessons/build-in-env.md)
- [本地源码手动编译应用](/docs/pages/guide/lessons/build-offline-src.md)
- [使用 git&patch 编译应用](/docs/pages/guide/lessons/build-git-patch.md)
- [玲珑应用自动化测试套件](/docs/pages/guide/lessons/test-with-toolchains.md)

更多课程可参考如意玲珑官网：<https://linyaps.org.cn/learn>

## :hammer_and_pick: 参与

我们鼓励您报告问题并贡献更改。查看 [构建指南](./BUILD.zh_CN.md) 以获取从源代码构建 linyaps 的说明。

您可以在 [Discussions](https://github.com/OpenAtom-Linyaps/linyaps/discussions) 上发起话题讨论。

## :balance_scale: 许可证

本项目使用 [LGPL-3.0-or-later](LICENSE) 许可发布。

## :busts_in_silhouette: 社区和贡献

感谢所有已经做出贡献的人！请参阅我们的[社区页面](https://linyaps.org.cn/community-charter)。

[![Contributors](https://contributors-img.web.app/image?repo=OpenAtom-Linyaps/linyaps)](https://github.com/OpenAtom-Linyaps/linyaps/graphs/contributors)

若如意玲珑项目对你有所帮助，或者你觉得它有用，欢迎点击该项目的[![ Star](https://img.shields.io/github/stars/OpenAtom-Linyaps/linyaps?style=social)](https://github.com/OpenAtom-Linyaps/linyaps/stargazers) 和 [![Fork](https://img.shields.io/github/forks/OpenAtom-Linyaps/linyaps?style=social)](https://github.com/OpenAtom-Linyaps/linyaps/network/members) 图标。
