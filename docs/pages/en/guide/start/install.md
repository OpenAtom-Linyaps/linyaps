<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# Install linyaps

linyaps is composed of three parts.

- ll-builder is used to build and debug linyaps applications, provided by linglong-builder.
- ll-box is a sandbox container, provided by linglong-box.
- ll-cli manages and runs linyaps applications, provided by linglong-bin.

## deepin v23

```bash
sudo apt install linglong-builder linglong-box linglong-bin
```

## UOS 1070

Add linyaps repository source.

```bash
echo "deb [trusted=yes] https://ci.deepin.com/repo/deepin/deepin-community/linglong-repo/ unstable main" | sudo tee -a /etc/apt/sources.list
```

Update the repository and install linyaps.

```bash
sudo apt update
sudo apt install linglong-builder linglong-box linglong-bin
```

## OpenEuler 24.03

Add linyaps repository source.

```bash
sudo curl -o /etc/yum.repos.d/linglong.repo -L https://eur.openeuler.openatom.cn/coprs/kamiyadm/linglong/repo/openeuler-24.03_LTS/kamiyadm-linglong-openeuler-24.03_LTS.repo
```

Update the repository and install linyaps.

```
sudo dnf update
sudo dnf install linglong-builder linglong-box linglong-bin
```

## Ubuntu 24.04

Add linyaps repository source.

```bash
sudo apt install -y apt-transport-https ca-certificates curl gpg xdg-utils
sudo mkdir -p /etc/apt/keyrings/
curl -fsSL https://download.opensuse.org/repositories/home:/kamiyadm/xUbuntu_24.04/Release.key | sudo gpg --dearmor -o /etc/apt/keyrings/linglong-apt-keyring.gpg
echo "deb [signed-by=/etc/apt/keyrings/linglong-apt-keyring.gpg] https://download.opensuse.org/repositories/home:/kamiyadm/xUbuntu_24.04/ ./" | sudo tee /etc/apt/sources.list.d/linglong.list
```

Update the repository and install linyaps.

```bash
sudo apt update
sudo apt install linglong-builder linglong-box linglong-bin
```

## debian 12

Add linyaps repository source.

```bash
sudo apt install -y apt-transport-https ca-certificates curl gpg xdg-utils
sudo mkdir -p /etc/apt/keyrings/
curl -fsSL https://download.opensuse.org/repositories/home:/kamiyadm/Debian_12/Release.key | sudo gpg --dearmor -o /etc/apt/keyrings/linglong-apt-keyring.gpg
echo "deb [signed-by=/etc/apt/keyrings/linglong-apt-keyring.gpg] https://download.opensuse.org/repositories/home:/kamiyadm/Debian_12/ ./" | sudo tee /etc/apt/sources.list.d/linglong.list
```

Update the repository and install linyaps.

```bash
sudo apt update
sudo apt install linglong-builder linglong-box linglong-bin
```

## openkylin 2.0rc

Add linyaps repository source.

```bash
sudo bash -c "echo 'deb [trusted=yes] https://ci.deepin.com/repo/obs/linglong:/multi_distro/openkylin2.0_repo/ ./' > /etc/apt/sources.list.d/linglong.list"
```

Update the repository and install linyaps.

```bash
sudo apt update
sudo apt install linglong-builder linglong-box linglong-bin
```

# Install the Pica tool

This tool currently provides the capability to convert DEB packages into linyaps packages. Generate the required `linglong.yaml` file for building linyaps applications and rely on `ll-builder` to implement application build and export.

## deepin v23

```bash
sudo apt install linglong-pica
```

## UOS 1070

The repository source needs to be added, which has been done previously.

```bash
sudo apt install linglong-pica
```
