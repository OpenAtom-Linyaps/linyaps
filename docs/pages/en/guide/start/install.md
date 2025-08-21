<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# Install linyaps

linyaps is composed of three parts.

- ll-builder is used to build and debug linyaps applications, provided by linglong-builder.
- ll-box is a sandbox container, provided by linglong-box.
- ll-cli manages and runs linyaps applications, provided by linglong-bin.

## Repository Usage Instructions

### Release Repository

Automatically built based on the latest tag.

Repository URL: <https://ci.deepin.com/repo/obs/linglong:/CI:/release>

Build URL: <https://build.deepin.com/project/show/linglong:CI:release>

### Latest Repository

Automatically built based on the latest commits.

Repository URL: <https://ci.deepin.com/repo/obs/linglong:/CI:/latest>

Build URL: <https://build.deepin.com/project/show/linglong:CI:latest>

:::tip

The installation steps below are based on the release repository. If you'd like to experience unreleased features, you can replace release in the repository URL with latest to install the preview version built from the master branch.

:::

## Linyaps Installation Instructions

### Arch / Manjaro / Parabola Linux

```sh
sudo pacman -Syu linyaps
```

The Linyaps Web Store installation tool needs to be installed through the [AUR repository](https://aur.archlinux.org/packages/linyaps-web-store-installer) or [self-built source repository](https://github.com/taotieren/aur-repo).

```bash
# AUR
yay -Syu linyaps-web-store-installer
# 或自建源
sudo pacman -Syu linyaps-web-store-installer
```

### deepin 25

```sh
echo "deb [trusted=yes] https://ci.deepin.com/repo/obs/linglong:/CI:/release/Deepin_25/ ./" | sudo tee /etc/apt/sources.list.d/linglong.list
sudo apt update
sudo apt install linglong-bin linglong-installer
```

### deepin 23

```sh
echo "deb [trusted=yes] https://ci.deepin.com/repo/obs/linglong:/CI:/release/Deepin_23/ ./" | sudo tee /etc/apt/sources.list.d/linglong.list
sudo apt update
sudo apt install linglong-bin linglong-installer
```

### Fedora 41

```sh
sudo dnf config-manager addrepo --from-repofile "https://ci.deepin.com/repo/obs/linglong:/CI:/release/Fedora_41/linglong%3ACI%3Arelease.repo"
sudo dnf update
sudo dnf install linglong-bin linyaps-web-store-installer
```

### Fedora 42

```sh
sudo dnf config-manager addrepo --from-repofile "https://ci.deepin.com/repo/obs/linglong:/CI:/release/Fedora_42/linglong%3ACI%3Arelease.repo"
sudo dnf update
sudo dnf install linglong-bin linyaps-web-store-installer
```

### Ubuntu 24.04

```sh
echo "deb [trusted=yes] https://ci.deepin.com/repo/obs/linglong:/CI:/release/xUbuntu_24.04/ ./" | sudo tee /etc/apt/sources.list.d/linglong.list
sudo apt update
sudo apt install linglong-bin linglong-installer
```

### Debian 12

```sh
echo "deb [trusted=yes] https://ci.deepin.com/repo/obs/linglong:/CI:/release/Debian_12/ ./" | sudo tee /etc/apt/sources.list.d/linglong.list
sudo apt update
sudo apt install linglong-bin linglong-installer
```

### Debian 13

```sh
echo "deb [trusted=yes] https://ci.deepin.com/repo/obs/linglong:/CI:/release/Debian_13/ ./" | sudo tee /etc/apt/sources.list.d/linglong.list
sudo apt update
sudo apt install linglong-bin linglong-installer
```

### openEuler 23.09

```sh
sudo dnf config-manager --add-repo "https://ci.deepin.com/repo/obs/linglong:/CI:/release/openEuler_23.09/linglong%3ACI%3Arelease.repo"
sudo sh -c "echo gpgcheck=0 >> /etc/yum.repos.d/linglong%3ACI%3Arelease.repo"
sudo dnf update
sudo dnf install linglong-bin linyaps-web-store-installer
```

### openEuler 24.03

```sh
sudo dnf config-manager --add-repo "https://ci.deepin.com/repo/obs/linglong:/CI:/release/openEuler_24.03/linglong%3ACI%3Arelease.repo"
sudo sh -c "echo gpgcheck=0 >> /etc/yum.repos.d/linglong%3ACI%3Arelease.repo"
sudo dnf update
sudo dnf install linglong-bin linyaps-web-store-installer
```

### UOS 1070

```sh
echo "deb [trusted=yes] https://ci.deepin.com/repo/obs/linglong:/CI:/release/uos_1070/ ./" | sudo tee /etc/apt/sources.list.d/linglong.list
sudo apt update
sudo apt install linglong-bin linglong-installer
```

### AnolisOS 8

```sh
sudo dnf config-manager addrepo --from-repofile "https://ci.deepin.com/repo/obs/linglong:/CI:/release/AnolisOS_8/linglong%3ACI%3Arelease.repo"
sudo dnf update
sudo dnf install linglong-bin linyaps-web-store-installer
```

### openkylin 2.0

```sh
echo "deb [trusted=yes] https://ci.deepin.com/repo/obs/linglong:/CI:/release/openkylin_2.0/ ./" | sudo tee /etc/apt/sources.list.d/linglong.list
sudo apt update
sudo apt install linglong-bin linglong-installer
```

## Linyaps Build Tool Installation Instructions

### Debian Base

```bash
sudo apt install linglong-builder
```

### RPM Base

```bash
sudo dnf install linglong-builder
```
