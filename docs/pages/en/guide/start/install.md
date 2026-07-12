<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# Install Linyaps

linyaps is composed of three parts:

- ll-builder is used to build and debug Linyaps applications, provided by linglong-builder.
- ll-box sandbox container, provided by linglong-box.
- ll-cli manages and runs Linyaps applications, provided by linglong-bin.

## Repository Usage Instructions

### release repository

Automatically built based on the latest tag

1. Repository address <https://ci.deepin.com/repo/obs/linglong:/CI:/release>
2. Build address <https://build.deepin.com/project/show/linglong:CI:release>

### latest repository

Automatically built based on the latest commit

1. Repository address <https://ci.deepin.com/repo/obs/linglong:/CI:/latest>
2. Build address <https://build.deepin.com/project/show/linglong:CI:latest>

:::tip

The following installation steps all use the release repository. If you want to experience features that have not yet been released, change "release" to "latest" in the repository address to install the preview version built based on the master branch

:::

## Linyaps Installation Instructions

### Arch / Manjaro / Parabola Linux

```sh
sudo pacman -Syu linyaps
```

Linyaps web store installer needs to be installed through [AUR repository](https://aur.archlinux.org/packages/linyaps-web-store-installer) or [self-built repository](https://github.com/taotieren/aur-repo).

```bash
# AUR
yay -Syu linyaps-web-store-installer
# or self-built source
sudo pacman -Syu linyaps-web-store-installer
```

### Deepin 25

```sh
echo "deb [trusted=yes] https://ci.deepin.com/repo/obs/linglong:/CI:/release/Deepin_25/ ./" | sudo tee /etc/apt/sources.list.d/linglong.list
sudo apt update
sudo apt install linglong-bin
```

### Deepin 23

```sh
echo "deb [trusted=yes] https://ci.deepin.com/repo/obs/linglong:/CI:/release/Deepin_23/ ./" | sudo tee /etc/apt/sources.list.d/linglong.list
sudo apt update
sudo apt install linglong-bin
```

### Fedora 42

```sh
sudo dnf config-manager addrepo --from-repofile "https://ci.deepin.com/repo/obs/linglong:/CI:/release/Fedora_42/linglong%3ACI%3Arelease.repo"
sudo dnf update
sudo dnf install linglong-bin
```

### Fedora 43

```sh
sudo dnf config-manager addrepo --from-repofile "https://ci.deepin.com/repo/obs/linglong:/CI:/release/Fedora_43/linglong%3ACI%3Arelease.repo"
sudo dnf update
sudo dnf install linglong-bin
```

### Ubuntu 25.10

```sh
echo "deb [trusted=yes] https://ci.deepin.com/repo/obs/linglong:/CI:/release/Ubuntu_25.10/ ./" | sudo tee /etc/apt/sources.list.d/linglong.list
sudo apt update
sudo apt install linglong-bin
```

### Ubuntu 25.04

```sh
echo "deb [trusted=yes] https://ci.deepin.com/repo/obs/linglong:/CI:/release/Ubuntu_25.04/ ./" | sudo tee /etc/apt/sources.list.d/linglong.list
sudo apt update
sudo apt install linglong-bin
```

### Ubuntu 24.04

```sh
echo "deb [trusted=yes] https://ci.deepin.com/repo/obs/linglong:/CI:/release/xUbuntu_24.04/ ./" | sudo tee /etc/apt/sources.list.d/linglong.list
sudo apt update
sudo apt install linglong-bin
```

### Debian 13

```sh
echo "deb [trusted=yes] https://ci.deepin.com/repo/obs/linglong:/CI:/release/Debian_13/ ./" | sudo tee /etc/apt/sources.list.d/linglong.list
sudo apt update
sudo apt install linglong-bin
```

### Debian 12

```sh
echo "deb [trusted=yes] https://ci.deepin.com/repo/obs/linglong:/CI:/release/Debian_12/ ./" | sudo tee /etc/apt/sources.list.d/linglong.list
sudo apt update
sudo apt install linglong-bin
```

### openEuler 25.03

```sh
sudo dnf config-manager --add-repo "https://ci.deepin.com/repo/obs/linglong:/CI:/release/openEuler_25.03/linglong%3ACI%3Arelease.repo"
sudo sh -c "echo gpgcheck=0 >> /etc/yum.repos.d/linglong%3ACI%3Arelease.repo"
sudo dnf update
sudo dnf install linglong-bin
```

### openEuler 24.03 SP2

```sh
sudo dnf config-manager --add-repo "https://ci.deepin.com/repo/obs/linglong:/CI:/release/openEuler_24.03/linglong%3ACI%3Arelease.repo"
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

### AnolisOS 23.4

```sh
sudo dnf config-manager addrepo --from-repofile "https://ci.deepin.com/repo/obs/linglong:/CI:/release/AnolisOS_23.4/linglong%3ACI%3Arelease.repo"
sudo dnf update
sudo dnf install linglong-bin
```

### AnolisOS 23.3

```sh
sudo dnf config-manager addrepo --from-repofile "https://ci.deepin.com/repo/obs/linglong:/CI:/release/AnolisOS_23.3/linglong%3ACI%3Arelease.repo"
sudo dnf update
sudo dnf install linglong-bin
```

### openkylin 2.0

```sh
echo "deb [trusted=yes] https://ci.deepin.com/repo/obs/linglong:/CI:/release/openkylin_2.0/ ./" | sudo tee /etc/apt/sources.list.d/linglong.list
sudo apt update
sudo apt install linglong-bin
```

### NixOS

In NixOS 25.11 or later versions, modify the configuration file (usually `/etc/nixos/configuration.nix`), and add:

```nix
  services.linyaps.enable = true;
```

## Linyaps Build Tool Installation Instructions

### Debian-based

```bash
sudo apt install linglong-builder
```

### RPM-based

```bash
sudo dnf install linglong-builder
```

## Linyaps Conversion Tool Installation Instructions

### Deepin 23/25

```bash
sudo apt install linglong-pica

```

### Arch Linux

Install via [AUR repository](https://aur.archlinux.org/packages/linglong-pica) or [self-hosted repository](https://github.com/taotieren/aur-repo).

```bash

# AUR
yay -Syu linglong-pica

# or self-hosted repository

sudo pacman -Syu linglong-pica

```
