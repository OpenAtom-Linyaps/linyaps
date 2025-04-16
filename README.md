# Linyaps: **Next-Gen Universal Package Manager for Linux**

## :package: Introduction

[English](README.md) | [简体中文](README.zh_CN.md)

[![Contributors](https://img.shields.io/github/contributors/OpenAtom-Linyaps/linyaps)](https://github.com/OpenAtom-Linyaps/linyaps/graphs/contributors)
[![Latest Release](https://img.shields.io/github/v/release/OpenAtom-Linyaps/linyaps?style=flat&color=brightgreen)](https://github.com/OpenAtom-Linyaps/linyaps/releases)
[![Powered by Linyaps](https://img.shields.io/badge/powered%20by-Linyaps-ff69b4)](https://github.com/OpenAtom-Linyaps/linyaps)
[![Build Status](https://build.deepin.com/projects/linglong:CI:latest/packages/linyaps/badge.svg?type=default)](https://build.deepin.com/projects/linglong:CI:latest)

[![GitHub Stars](https://img.shields.io/github/stars/OpenAtom-Linyaps/linyaps?style=social)](https://github.com/OpenAtom-Linyaps/linyaps/stargazers)
[![GitHub Forks](https://img.shields.io/github/forks/OpenAtom-Linyaps/linyaps?style=social&label=Fork)](https://github.com/OpenAtom-Linyaps/linyaps/network/members)
[![Code Size](https://img.shields.io/github/languages/code-size/OpenAtom-Linyaps/linyaps)](https://github.com/OpenAtom-Linyaps/linyaps)
[![GitHub Issues](https://img.shields.io/github/issues/OpenAtom-Linyaps/linyaps?style=social)](https://github.com/OpenAtom-Linyaps/linyaps/issues)

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

### UOS 1070 (Latest repository only)

```sh
echo "deb [trusted=yes] https://ci.deepin.com/repo/obs/linglong:/CI:/latest/uos_1070/ ./" | sudo tee /etc/apt/sources.list.d/linglong.list
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

### Command-line Tools

- [Introduction](./docs/pages/en/guide/ll-cli/introduction.md)
- [Install](./docs/pages/en/guide/ll-cli/install.md)
- [Run](./docs/pages/en/guide/ll-cli/run.md)
- [Uninstall](./docs/pages/en/guide/ll-cli/uninstall.md)
- [Upgrade](./docs/pages/en/guide/ll-cli/update.md)
- [List](./docs/pages/en/guide/ll-cli/list.md)
- [Prune](./docs/pages/en/guide/ll-cli/prune.md)
- [Exec](./docs/pages/en/guide/ll-cli/exec.md)
- [Content](./docs/pages/en/guide/ll-cli/content.md)
- [Info](./docs/pages/en/guide/ll-cli/info.md)
- [PS](./docs/pages/en/guide/ll-cli/ps.md)
- [Kill](./docs/pages/en/guide/ll-cli/kill.md)
- [Search](./docs/pages/en/guide/ll-cli/query.md)

### Build Tools

- [Introduction](./docs/pages/en/guide/ll-builder/introduction.md)
- [Demo](./docs/pages/en/guide/ll-builder/demo.md)
- [Create](./docs/pages/en/guide/ll-builder/create.md)
- [Run](./docs/pages/en/guide/ll-builder/run.md)
- [Push](./docs/pages/en/guide/ll-builder/push.md)
- [Export](./docs/pages/en/guide/ll-builder/export.md)

### Package Conversion Tools

#### deb Conversion

- [Introduction](./docs/pages/en/guide/ll-pica/introduction.md)
- [Init](./docs/pages/en/guide/ll-pica/init.md)
- [Convert](./docs/pages/en/guide/ll-pica/convert.md)
- [Adep](./docs/pages/en/guide/ll-pica/adep.md)

#### AppImage Conversion

- [Introduction](./docs/pages/en/guide/ll-appimage-convert/introduction.md)
- [Convert](./docs/pages/en/guide/ll-appimage-convert/convert-appimage.md)

#### Flatpak Conversion

- [Introduction](./docs/pages/en/guide/ll-flatpak-convert/introduction.md)
- [Convert](./docs/pages/en/guide/ll-flatpak-convert/convert-flatpak.md)

### Debugging

- [Debug](/docs/pages/en/guide/debug/debug.md)

### FAQs

- [Runtime FAQs](/docs/pages/en/guide/debug/faq.md)
- [Linyaps Builder FAQs](/docs/pages/en/guide/debug/ll-builder-faq.md)
- [Linyaps Conversion Tool FAQs](/docs/pages/en/guide/debug/ll-pica-faq.md)

## :book: Learning Resources

### Related Projects

- [OSTree](https://github.com/ostreedev/ostree)
- [Linyaps Packaging Tool - ll-killer-go](https://github.com/System233/ll-killer-go)
- [Linyaps Web Store](https://github.com/yoloke/Linglong-Shop)

### Tutorial Series

- [Linyaps Packaging Basics](/docs/pages/guide/lessons/basic-notes.md)
- [Manual Compilation in Container](/docs/pages/guide/lessons/build-in-env.md)
- [Offline Source Compilation](/docs/pages/guide/lessons/build-offline-src.md)
- [Compilation with Git & Patch](/docs/pages/guide/lessons/build-git-patch.md)
- [Automated Testing Suite](/docs/pages/guide/lessons/test-with-toolchains.md)

Explore more tutorials at [Linyaps Official Website](https://linyaps.org.cn/learn).

## :hammer_and_pick: Contribution

We welcome issue reports and contributions. See the [Build Guide](./BUILD.md) for instructions on building Linyaps from source.

Start discussions on [GitHub Discussions](https://github.com/OpenAtom-Linyaps/linyaps/discussions).

## :balance_scale: License

Licensed under [LGPL-3.0-or-later](LICENSE).

## :busts_in_silhouette: Community

Acknowledgment to all contributors! Visit our [Community Page](https://linyaps.org.cn/community-charter).

[![Contributors](https://contributors-img.web.app/image?repo=OpenAtom-Linyaps/linyaps)](https://github.com/OpenAtom-Linyaps/linyaps/graphs/contributors)

If Linyaps helps you, consider giving it a [![Star](https://img.shields.io/github/stars/OpenAtom-Linyaps/linyaps?style=social)](https://github.com/OpenAtom-Linyaps/linyaps/stargazers) or [![Fork](https://img.shields.io/github/forks/OpenAtom-Linyaps/linyaps?style=social)](https://github.com/OpenAtom-Linyaps/linyaps/network/members).
