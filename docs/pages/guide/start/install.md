<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# 安装玲珑

玲珑由三部分组成：

- ll-builder 用来构建和调试玲珑应用，由 linglong-builder 提供。
- ll-box 沙箱容器，由 linglong-box 提供。
- ll-cli 管理和运行玲珑应用，由 linglong-bin 提供。

## deepin v23

```bash
sudo apt install linglong-builder linglong-box linglong-bin
```

## UOS 1070

添加玲珑仓库源。

```bash
echo "deb [trusted=yes] https://ci.deepin.com/repo/deepin/deepin-community/linglong-repo/ unstable main" | sudo tee -a /etc/apt/sources.list
```

更新仓库并安装玲珑。

```bash
sudo apt update
sudo apt install linglong-builder linglong-box linglong-bin
```

## OpenEuler 24.03

添加玲珑仓库源。

```bash
sudo curl -o /etc/yum.repos.d/linglong.repo -L https://eur.openeuler.openatom.cn/coprs/kamiyadm/linglong/repo/openeuler-24.03_LTS/kamiyadm-linglong-openeuler-24.03_LTS.repo
```

更新仓库并安装玲珑。

```bash
sudo dnf update
sudo dnf install linglong-builder linglong-box linglong-bin
```

## Ubuntu 24.04

添加玲珑仓库源。

```bash
sudo apt install -y apt-transport-https ca-certificates curl gpg xdg-utils
sudo mkdir -p /etc/apt/keyrings/
curl -fsSL https://download.opensuse.org/repositories/home:/kamiyadm/xUbuntu_24.04/Release.key | sudo gpg --dearmor -o /etc/apt/keyrings/linglong-apt-keyring.gpg
echo "deb [signed-by=/etc/apt/keyrings/linglong-apt-keyring.gpg] https://download.opensuse.org/repositories/home:/kamiyadm/xUbuntu_24.04/ ./" | sudo tee /etc/apt/sources.list.d/linglong.list
```

更新仓库并安装玲珑。

```bash
sudo apt update
sudo apt install linglong-builder linglong-box linglong-bin
```

## debian 12

添加玲珑仓库源。

```bash
sudo apt install -y apt-transport-https ca-certificates curl gpg xdg-utils
sudo mkdir -p /etc/apt/keyrings/
curl -fsSL https://download.opensuse.org/repositories/home:/kamiyadm/Debian_12/Release.key | sudo gpg --dearmor -o /etc/apt/keyrings/linglong-apt-keyring.gpg
echo "deb [signed-by=/etc/apt/keyrings/linglong-apt-keyring.gpg] https://download.opensuse.org/repositories/home:/kamiyadm/Debian_12/ ./" | sudo tee /etc/apt/sources.list.d/linglong.list
```

更新仓库并安装玲珑。

```bash
sudo apt update
sudo apt install linglong-builder linglong-box linglong-bin
```

## openkylin 2.0rc

添加玲珑仓库源。

```bash
sudo bash -c "echo 'deb [trusted=yes] https://ci.deepin.com/repo/obs/linglong:/multi_distro/openkylin2.0_repo/ ./' > /etc/apt/sources.list.d/linglong.list"
```

更新仓库并安装玲珑。

```bash
sudo apt update
sudo apt install linglong-builder linglong-box linglong-bin
```

# 安装 pica

本工具目前提供 deb 包转换为玲珑包的能力，生成构建玲珑应用需要的 linglong.yaml 文件，并依赖 ll-builder 来实现应用构建和导出。

## deepin v23

```bash
sudo apt install linglong-pica
```

## UOS 1070

需要添加仓库源，前面已添加。

```bash
sudo apt install linglong-pica
```
