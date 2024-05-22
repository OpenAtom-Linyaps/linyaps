<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# Install linglong

Linglong is composed of three parts.

- ll-builder is used to build and debug Linglong applications, provided by linglong-builder.
- ll-box is a sandbox container, provided by linglong-box.
- ll-cli manages and runs Linglong applications, provided by linglong-bin.

## deepin v23

```bash
sudo apt install linglong-builder
```

## UOS 1070

add Linglong repository source

```bash
echo "deb [trusted=yes] https://ci.deepin.com/repo/deepin/deepin-community/linglong-repo/ unstable main" | sudo tee -a /etc/apt/sources.list
```

```bash
sudo apt update
sudo apt install linglong-builder
```

# Install the Pica tool

This tool currently provides the capability to convert DEB packages into Linglong packages. Generate the required `linglong.yaml` file for building Linglong applications and rely on `ll-builder` to implement application build and export.

## deepin v23

```bash
sudo apt install linglong-pica
```

## UOS 1070

The repository source needs to be added, which has been done previously.

```bash
sudo apt install linglong-pica
```
