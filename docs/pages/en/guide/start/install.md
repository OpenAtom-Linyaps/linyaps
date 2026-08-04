<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# Install Linyaps

Some deepin and UOS releases already include Linyaps. Run `ll-cli --version`
first; if the command is available, you can continue to
[Quick Start](./quick-start.md). The installer below will
update an existing installation when a newer package is available.

The main command-line tools are:

- `ll-cli` manages and runs Linyaps applications and is provided by
  `linglong-bin`.
- `ll-builder` builds and debugs Linyaps applications and is provided by
  `linglong-builder`.

## Repository usage

### Release repository

This repository is built automatically from the latest release tag:

1. Repository: <https://ci.deepin.com/repo/obs/linglong:/CI:/release>
2. Build status: <https://build.deepin.com/project/show/linglong:CI:release>

### Latest repository

This repository is built automatically from the latest commit:

1. Repository: <https://ci.deepin.com/repo/obs/linglong:/CI:/latest>
2. Build status: <https://build.deepin.com/project/show/linglong:CI:latest>

:::tip

The following installation steps use the stable `release` repository. To test
unreleased changes, replace `release` with `latest` in an OBS repository URL.
The `latest` repository may contain incomplete changes and is not recommended
for production systems.

:::

## Install or update automatically

Review the script before running it, then install Linyaps with:

```sh
curl -fsSL https://get.linyaps.org.cn | sh
```

The script uses the distribution's native package when it is tracked by the
[Packaging status](https://repology.org/project/linyaps/versions) link on the
project home page (Arch Linux, Manjaro, Parabola Linux, and AOSC OS). For the
other distributions listed below, it configures the official Linyaps release
repository. Running the command again refreshes package metadata and updates an
existing installation.

The repositories below currently use HTTPS transport without package-signing
metadata. The script therefore configures the same trust policy as the manual
commands (`trusted=yes` for APT and `gpgcheck=0` for DNF).

NixOS is intentionally excluded from the automatic path because its system
configuration is declarative. The script detects NixOS and prints the required
configuration instead of modifying it.

## Install or update manually

### Arch / Manjaro / Parabola Linux

```sh
sudo pacman -Syu --needed linyaps
```

Linyaps web store installer needs to be installed through [AUR repository](https://aur.archlinux.org/packages/linyaps-web-store-installer) or [self-built repository](https://github.com/taotieren/aur-repo).

```bash
# AUR
yay -Syu linyaps-web-store-installer
# or self-built source
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

### Fedora 42

```sh
sudo dnf config-manager addrepo --from-repofile "https://ci.deepin.com/repo/obs/linglong:/CI:/release/Fedora_42/linglong%3ACI%3Arelease.repo"
sudo sh -c "printf '\ngpgcheck=0\n' >> '/etc/yum.repos.d/linglong%3ACI%3Arelease.repo'"
sudo dnf makecache --refresh
sudo dnf install linglong-bin linyaps-web-store-installer
```

### Fedora 43

```sh
sudo dnf config-manager addrepo --from-repofile "https://ci.deepin.com/repo/obs/linglong:/CI:/release/Fedora_43/linglong%3ACI%3Arelease.repo"
sudo sh -c "printf '\ngpgcheck=0\n' >> '/etc/yum.repos.d/linglong%3ACI%3Arelease.repo'"
sudo dnf makecache --refresh
sudo dnf install linglong-bin linyaps-web-store-installer
```

### Ubuntu 24.04

```sh
echo "deb [trusted=yes] https://ci.deepin.com/repo/obs/linglong:/CI:/release/xUbuntu_24.04/ ./" | sudo tee /etc/apt/sources.list.d/linglong.list
sudo apt update
sudo apt install linglong-bin linglong-installer
```

### Ubuntu 25.04

```sh
echo "deb [trusted=yes] https://ci.deepin.com/repo/obs/linglong:/CI:/release/Ubuntu_25.04/ ./" | sudo tee /etc/apt/sources.list.d/linglong.list
sudo apt update
sudo apt install linglong-bin linglong-installer
```

### Ubuntu 25.10

```sh
echo "deb [trusted=yes] https://ci.deepin.com/repo/obs/linglong:/CI:/release/Ubuntu_25.10/ ./" | sudo tee /etc/apt/sources.list.d/linglong.list
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

### openEuler 24.03

```sh
sudo dnf config-manager --add-repo "https://ci.deepin.com/repo/obs/linglong:/CI:/release/openEuler_24.03/linglong%3ACI%3Arelease.repo"
sudo sh -c "printf '\ngpgcheck=0\n' >> '/etc/yum.repos.d/linglong%3ACI%3Arelease.repo'"
sudo dnf makecache --refresh
sudo dnf install linglong-bin linyaps-web-store-installer
```

### openEuler 25.03

```sh
sudo dnf config-manager --add-repo "https://ci.deepin.com/repo/obs/linglong:/CI:/release/openEuler_25.03/linglong%3ACI%3Arelease.repo"
sudo sh -c "printf '\ngpgcheck=0\n' >> '/etc/yum.repos.d/linglong%3ACI%3Arelease.repo'"
sudo dnf makecache --refresh
sudo dnf install linglong-bin linyaps-web-store-installer
```

### UOS 1070

```sh
echo "deb [trusted=yes] https://ci.deepin.com/repo/obs/linglong:/CI:/release/uos_1070/ ./" | sudo tee /etc/apt/sources.list.d/linglong.list
sudo apt update
sudo apt install linglong-bin linglong-installer
```

### Anolis OS 23.3

```sh
sudo dnf config-manager --add-repo "https://ci.deepin.com/repo/obs/linglong:/CI:/release/AnolisOS_23.3/linglong%3ACI%3Arelease.repo"
sudo sh -c "printf '\ngpgcheck=0\n' >> '/etc/yum.repos.d/linglong%3ACI%3Arelease.repo'"
sudo dnf makecache --refresh
sudo dnf install linglong-bin linyaps-web-store-installer
```

### Anolis OS 23.4

```sh
sudo dnf config-manager --add-repo "https://ci.deepin.com/repo/obs/linglong:/CI:/release/AnolisOS_23.4/linglong%3ACI%3Arelease.repo"
sudo sh -c "printf '\ngpgcheck=0\n' >> '/etc/yum.repos.d/linglong%3ACI%3Arelease.repo'"
sudo dnf makecache --refresh
sudo dnf install linglong-bin linyaps-web-store-installer
```

### openKylin 2.0

```sh
echo "deb [trusted=yes] https://ci.deepin.com/repo/obs/linglong:/CI:/release/openkylin_2.0/ ./" | sudo tee /etc/apt/sources.list.d/linglong.list
sudo apt update
sudo apt install linglong-bin linglong-installer
```

### AOSC OS

```sh
sudo oma refresh
sudo oma install linyaps
```

### NixOS

On NixOS 25.11 or later, add the following option to the system configuration
(usually `/etc/nixos/configuration.nix`):

```nix
services.linyaps.enable = true;
```

Then install or update Linyaps by rebuilding the system:

```sh
sudo nixos-rebuild switch --upgrade
```

## Install the Linyaps build tool

### Debian-based

```bash
sudo apt install linglong-builder
```

### RPM-based

```bash
sudo dnf install linglong-builder
```

## Install the Linyaps conversion tool

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
