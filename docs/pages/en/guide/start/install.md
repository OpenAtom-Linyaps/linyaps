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

## Installation Instructions

### deepin 25

```sh
echo "deb [trusted=yes] https://ci.deepin.com/repo/obs/linglong:/CI:/release/Deepin_25/ ./" | sudo tee /etc/apt/sources.list.d/linglong.list
sudo apt update
sudo apt install linglong-builder linglong-box linglong-bin
```

### deepin 23

```sh
echo "deb [trusted=yes] https://ci.deepin.com/repo/obs/linglong:/CI:/release/Deepin_23/ ./" | sudo tee /etc/apt/sources.list.d/linglong.list
sudo apt update
sudo apt install linglong-builder linglong-box linglong-bin
```

### Fedora 41

```sh
sudo dnf config-manager addrepo --from-repofile "https://ci.deepin.com/repo/obs/linglong:/CI:/release/Fedora_41/linglong%3ACI%3Arelease.repo"
sudo dnf update
sudo dnf install linglong-builder linglong-box linglong-bin
```

### Fedora 42

```sh
sudo dnf config-manager addrepo --from-repofile "https://ci.deepin.com/repo/obs/linglong:/CI:/release/Fedora_42/linglong%3ACI%3Arelease.repo"
sudo dnf update
sudo dnf install linglong-bin
```

### Ubuntu 24.04

```sh
echo "deb [trusted=yes] https://ci.deepin.com/repo/obs/linglong:/CI:/release/xUbuntu_24.04/ ./" | sudo tee /etc/apt/sources.list.d/linglong.list
sudo apt update
sudo apt install linglong-builder linglong-box linglong-bin
```

### Debian 12

```sh
echo "deb [trusted=yes] https://ci.deepin.com/repo/obs/linglong:/CI:/release/Debian_12/ ./" | sudo tee /etc/apt/sources.list.d/linglong.list
sudo apt update
sudo apt install linglong-builder linglong-box linglong-bin
```

### Debian 13

```sh
echo "deb [trusted=yes] https://ci.deepin.com/repo/obs/linglong:/CI:/release/Debian_13/ ./" | sudo tee /etc/apt/sources.list.d/linglong.list
sudo apt update
sudo apt install linglong-bin
```

### openEuler 23.09

```sh
sudo dnf config-manager --add-repo "https://ci.deepin.com/repo/obs/linglong:/CI:/release/openEuler_23.09/linglong%3ACI%3Arelease.repo"
sudo sh -c "echo gpgcheck=0 >> /etc/yum.repos.d/linglong%3ACI%3Arelease.repo"
sudo dnf update
sudo dnf install linglong-builder linglong-box linglong-bin
```

### openEuler 24.03

```sh
sudo dnf config-manager --add-repo "https://ci.deepin.com/repo/obs/linglong:/CI:/release/openEuler_24.03/linglong%3ACI%3Arelease.repo"
sudo sh -c "echo gpgcheck=0 >> /etc/yum.repos.d/linglong%3ACI%3Arelease.repo"
sudo dnf update
sudo dnf install linglong-bin
```

### UOS 1070

```sh
echo "deb [trusted=yes] https://ci.deepin.com/repo/obs/linglong:/CI:/release/uos_1070/ ./" | sudo tee /etc/apt/sources.list.d/linglong.list
sudo apt update
sudo apt install linglong-builder linglong-box linglong-bin
```

### AnolisOS 8

```sh
sudo dnf config-manager addrepo --from-repofile "https://ci.deepin.com/repo/obs/linglong:/CI:/release/AnolisOS_8/linglong%3ACI%3Arelease.repo"
sudo dnf update
sudo dnf install linglong-builder linglong-box linglong-bin
```

### openkylin 2.0

```sh
echo "deb [trusted=yes] https://ci.deepin.com/repo/obs/linglong:/CI:/release/openkylin_2.0/ ./" | sudo tee /etc/apt/sources.list.d/linglong.list
sudo apt update
sudo apt install linglong-builder linglong-box linglong-bin
```
