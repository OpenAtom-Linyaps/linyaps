<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# Install Linglong Environment

## Kernel version requirement

Use `uname -r` to check the kernel version:

```bash
uname -r
```

Here is the output:

```text
5.10.101-amd64-desktop
```

Kernel version requirement >=4.19.

* The 4.19 kernel of the x86 architecture needs to enable the user namespace.

## Deb package installation tutorial

### deepin 20

Use `apt` to install the Linglong environment.

```bash
sudo apt install linglong-builder \
                 linglong-box \
                 linglong-dbus-proxy \
                 linglong-bin \
                 linglong-installer
```

### deepin 23

Deepin 23 comes preinstalled with the Linglong environment.

## Deb download

### Debian 11 (bullseye)

[Click here to download Debian 11 (bullseye) Linglong deb package](https://github.com/linuxdeepin/linglong-hub/releases/download/1.3.3/debian_bullseye.tar.gz)

### Ubuntu 22.04 (jammy)

[Click here to download Ubuntu 22.04 (jammy) Linglong deb package](https://github.com/linuxdeepin/linglong-hub/releases/download/1.3.3/ubuntu_jammy.tar.gz)

### Update the repository source

Add the Tsinghua warehouse source for your distro to your sources:

* [Debian source](https://mirrors.tuna.tsinghua.edu.cn/help/debian/)

* [Ubuntu source](https://mirrors.tuna.tsinghua.edu.cn/help/ubuntu/)

### Deb install

The Debian 11 installation is as follows:

```bash
tar -zxvf debian_bullseye.tar.gz
sudo apt install ./*.deb
sudo reboot
```
