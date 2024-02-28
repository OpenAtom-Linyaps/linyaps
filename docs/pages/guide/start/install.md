<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# 安装玲珑环境

## 内核版本

使用 `uname -r` 查看内核版本。

```bash
uname -r
```

输出如下：

```text
5.10.101-amd64-desktop
```

内核版本要求>=4.19。

* x86架构4.19内核需要开启user namespace。

## deb包安装教程
### deepin v23

deepin v23 已预装玲珑环境。

### deb包下载

### debian 11(bullseye)

[点击下载debian 11(bullseye)玲珑deb包](https://github.com/linuxdeepin/linglong-hub/releases/download/1.3.3/debian_bullseye.tar.gz)

### ubuntu 22.04(jammy)

[点击下载ubuntu 22.04(jammy)玲珑deb包](https://github.com/linuxdeepin/linglong-hub/releases/download/1.3.3/ubuntu_jammy.tar.gz)

### 更新仓库源

根据系统版本将对应的清华仓库源添加到源配置文件中。

* [debian 添加清华仓库源教程](https://mirrors.tuna.tsinghua.edu.cn/help/debian/)

* [ubuntu 添加清华仓库源教程](https://mirrors.tuna.tsinghua.edu.cn/help/ubuntu/)

### deb包安装

debian 11 安装示例如下：

```bash
tar -zxvf debian_bullseye.tar.gz
sudo apt install ./*.deb
sudo reboot
```
