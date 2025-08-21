<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# 安装如意玲珑

如意玲珑由三部分组成：

- ll-builder 用来构建和调试如意玲珑应用，由 linglong-builder 提供。
- ll-box 沙箱容器，由 linglong-box 提供。
- ll-cli 管理和运行如意玲珑应用，由 linglong-bin 提供。

## 仓库使用说明

### release 仓库

基于最新tag自动构建

1.  仓库地址 <https://ci.deepin.com/repo/obs/linglong:/CI:/release>
2.  构建地址 <https://build.deepin.com/project/show/linglong:CI:release>

### latest 仓库

基于最新提交自动构建

1.  仓库地址 <https://ci.deepin.com/repo/obs/linglong:/CI:/latest>
2.  构建地址 <https://build.deepin.com/project/show/linglong:CI:latest>

:::tip

以下安装步骤均使用基于release仓库，如果想体验还未发布的功能，将仓库地址中的release更改为latest，即可安装基于master分支构建的预览版

:::

## 如意玲珑安装说明

### Arch / Manjaro / Parabola Linux

```sh
sudo pacman -Syu linyaps
```

如意玲珑网页商店安装工具需要通过 [AUR 仓库](https://aur.archlinux.org/packages/linyaps-web-store-installer)或[自建源仓库](https://github.com/taotieren/aur-repo)安装。

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

## 如意玲珑构建工具安装说明

### Debian系

```bash
sudo apt install linglong-builder
```

### RPM系

```bash
sudo dnf install linglong-builder
```
