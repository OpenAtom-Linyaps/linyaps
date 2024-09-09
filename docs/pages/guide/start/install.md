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

## openKylin 2.0

添加玲珑仓库源。

```bash
sudo bash -c "echo 'deb [trusted=yes] https://ci.deepin.com/repo/obs/linglong:/multi_distro/openkylin2.0_repo/ ./' > /etc/apt/sources.list.d/linglong.list"
```

更新仓库并安装玲珑。

```bash
sudo apt update
sudo apt install linglong-builder linglong-box linglong-bin
```

\* 注意，目前`openKylin 2.0`官方仓库`nile`源中收录了来自社区维护者共享的玲珑相关软件包；经过实测，这些软件包在安装后并不能正常发挥玲珑的完整功能，因此需要安装来自上述附加`玲珑仓库源`的版本。在安装时请注意甄别当前安装版本的来源，参考:

```
linglong-bin:
  已安装：1.5.8-1
  候选： 1.5.8-1
  版本列表：
 *** 1.5.8-1 500
        500 https://ci.deepin.com/repo/obs/linglong:/multi_distro/openkylin2.0_repo ./ Packages
        100 /var/lib/dpkg/status
     1.5.7-1ok1 500
        500 https://mirrors.aliyun.com/openkylin nile/main amd64 Packages
```

此处官方仓库`nile`提供的`linglong-bin`及相关依赖包在安装后无法正常使用。
\* 本结论仅适用于当前说明的`1.5.7-1ok1`版本，考虑到软件包的可迭代性，如后续出现其他较新版本，需以实际运行结果为准。

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
