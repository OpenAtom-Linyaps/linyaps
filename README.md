<!-- markdownlint-disable-next-line MD033 -->

# <img src="./misc/image/logo.svg" alt="Linyaps icon" width="24" height="24" style="vertical-align:middle;">Linyaps: A More Advanced Cross-Distribution Linux Package Management Toolkit

## :package: Introduction

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

[![Packaging status](https://repology.org/badge/vertical-allrepos/linyaps.svg)](https://repology.org/project/linyaps/versions)

**Linyaps** (Linyaps Is Not Yet Another Packaging System) is a **cross-distribution Linux package format** developed and open-sourced by the Linyaps community. It implements application packaging, management, and distribution through independent sandbox containers, aiming to replace traditional package managers like deb and rpm. Linyaps ensures Linux software runs with better compatibility, security, and efficiency.

### :sparkles: Highlights

- **Innovative Partial Runtime Design**: Based on a standardized sandbox Runtime, applications can be built once and run across all Linux distributions. Multiple Runtime versions coexist with shared files to reduce redundancy. Shared resources are reused during startup via dynamic libraries, **significantly improving speed and avoiding dependency conflicts**.
- **Non-Privileged Sandbox with Dual-Layer Isolation**: Runs without root privileges by default. Utilizes kernel Namespace isolation (process/filesystem/network) to create a **secure sandbox**. Atomic incremental updates and version rollbacks are provided via OSTree repositories, resulting in **lower resource consumption** compared to full sandbox solutions.

### :flags: Progress

- **Supported Distributions**: deepin, UOS, openEuler, Ubuntu, Debian, openKylin, Anolis OS. More distributions are under adaptation. Contributions are welcome.
- **CPU Architectures**: X86, ARM64, LoongArch. Future support for RISC-V and others.

## :gear: Installation

Installation instructions for supported distributions:

### deepin 23

Install:

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

## :rocket: Quick Start

```sh
ll-cli install cn.org.linyaps.demo
ll-cli run cn.org.linyaps.demo
```

## :dart: Motivation

Why develop Linyaps when Snap, Flatpak, and AppImage already exist?

- In 2017, we evaluated Flatpak and built 100+ packages, but discontinued due to large application size, excessive disk usage, and slow security fixes.
- Snap has compatibility issues outside Ubuntu and lacks an open ecosystem.
- AppImage offers portability but lacks centralized repository management and robust sandboxing, compromising security.

After extensive experimentation, we designed Linyaps to address these shortcomings. Key advantages over alternatives include:

- **Partial Runtime**: Smaller footprint and faster startup via shared resources.
- **Rootless Sandbox**: Enhanced security without requiring root privileges.

Benchmark results demonstrate Linyaps' performance superiority:

| Test #  | Linyaps Frames | Linyaps Time (ms) | Flatpak Frames | Flatpak Time (ms) | AppImage Frames | AppImage Time (ms) | Snap Frames | Snap Time (ms) |
| ------- | -------------- | ----------------- | -------------- | ----------------- | --------------- | ------------------ | ----------- | -------------- |
| 1       | 9              | 149.4             | 14             | 232.4             | 16              | 265.6              | 42          | 697.2          |
| 2       | 9              | 149.4             | 13             | 215.8             | 17              | 282.2              | 41          | 680.6          |
| 3       | 8              | 132.8             | 9              | 149.4             | 15              | 249                | 40          | 664            |
| 4       | 9              | 149.4             | 13             | 215.8             | 15              | 249                | 41          | 680.6          |
| 5       | 8              | 132.8             | 14             | 232.4             | 16              | 265.6              | 42          | 697.2          |
| 6       | 8              | 132.8             | 13             | 215.8             | 15              | 249                | 39          | 664            |
| 7       | 9              | 149.4             | 12             | 199.2             | 15              | 249                | 39          | 647.4          |
| 8       | 8              | 132.8             | 14             | 232.4             | 16              | 265.6              | 40          | 680.6          |
| **Avg** | 8.5            | 141.1             | 12.8           | 213.7             | 15.6            | 261.6              | 40.5        | 676.2          |

## :incoming_envelope: Getting Help

For assistance, use the following channels:

- [GitHub Issues](https://github.com/OpenAtom-Linyaps/linyaps/issues)
- [Forum](https://bbs.deepin.org/module/detail/230)
- [Contact Us](https://linyaps.org.cn/contactus)

## :memo: Documentation

### Getting Started

- [Overview](./docs/pages/en/guide/start/whatis.md) - What Linyaps is, its features, use cases, and how to use the documentation
- [Quick Start](./docs/pages/en/guide/start/quick-start.md) - Set up `ll-cli`, then search for, install, and run an application
- [Release Notes](./docs/pages/en/guide/start/release_note.md) - New features, bug fixes, and other important changes in each release

### User Guide

- [Install Linyaps](./docs/pages/en/guide/start/install.md) - Installation instructions for different Linux distributions
- [Manage Applications](./docs/pages/en/guide/start/manage-apps-with-cli.md) - Install, inspect, upgrade, uninstall, and manage application processes

#### Advanced

- [Runtime Configuration](./docs/pages/en/guide/extra/runtime_config.md) - Runtime configuration load order, examples, and field reference
- [Manage Runtimes](./docs/pages/en/guide/start/manage-runtimes-with-cli.md) - Inspect and clean up runtimes, analyze dependencies, and perform forced operations
- [Repository Management](./docs/pages/en/guide/publishing/repositories.md) - Repository configuration, priorities, and routine management

### Developer Guide

- [Build Your First Linyaps Application](./docs/pages/en/guide/start/build_your_first_app.md) - Configure a project and complete your first application build
- [ll-builder Workflow](./docs/pages/en/guide/start/ll-builder-workflow.md) - Create, build, debug, validate, export, and push a project

#### Build Examples

- [Understanding the Build Configuration](./docs/pages/en/guide/building/demo.md) - A section-by-section walkthrough of `linglong.yaml`
- [Convert a deb Package](./docs/pages/en/guide/building/deb_conversion.md) - Convert a deb package into a Linyaps application

- [Publish in UAB Format](./docs/pages/en/guide/publishing/uab.md) - Publish applications in UAB format
- [Desktop Integration Guide](./docs/pages/en/guide/desktop-integration/README.md) - Integrate Linyaps with desktop environments

#### Advanced

- [Debug a Linyaps Application](./docs/pages/en/guide/debug/debug.md) - Debug a Linyaps application with GDB
- [Module Management](./docs/pages/en/guide/building/modules.md) - Split, build, and install modules
- [Multi-architecture Support](./docs/pages/en/guide/building/multiarch.md) - Configure and perform native and cross-architecture builds

### Tutorial Series

- [Linyaps Packaging Basics](./docs/pages/en/guide/lessons/basic-notes.md)
- [Manual Compilation in Container](./docs/pages/en/guide/lessons/build-in-env.md)
- [Offline Source Compilation](./docs/pages/en/guide/lessons/build-offline-src.md)
- [Compilation with Git & Patch](./docs/pages/en/guide/lessons/build-git-patch.md)
- [Automated Testing Suite](./docs/pages/en/guide/lessons/test-with-toolchains.md)

### Related Projects

- [OSTree](https://github.com/ostreedev/ostree)
- [Linyaps Packaging Tool - ll-killer-go](https://github.com/System233/ll-killer-go)
- [Linyaps Web Store](https://github.com/yoloke/Linglong-Shop)

Explore more tutorials at [Linyaps Official Website](https://linyaps.org.cn/learn).

## :hammer_and_pick: Contribution

We welcome issue reports and contributions. See the [Developer Guide](./DEVELOPER_GUIDE.md) for instructions on building Linyaps from source.

Start discussions on [GitHub Discussions](https://github.com/OpenAtom-Linyaps/linyaps/discussions).

## :balance_scale: License

Licensed under [LGPL-3.0-or-later](LICENSE).

## :busts_in_silhouette: Community

Acknowledgment to all contributors! Visit our [Community Page](https://linyaps.org.cn/community-charter).

[![Contributors](https://contributors-img.web.app/image?repo=OpenAtom-Linyaps/linyaps)](https://github.com/OpenAtom-Linyaps/linyaps/graphs/contributors)

If Linyaps helps you, consider giving it a [![Star](https://img.shields.io/github/stars/OpenAtom-Linyaps/linyaps?style=social)](https://github.com/OpenAtom-Linyaps/linyaps/stargazers) or [![Fork](https://img.shields.io/github/forks/OpenAtom-Linyaps/linyaps?style=social)](https://github.com/OpenAtom-Linyaps/linyaps/network/members).
