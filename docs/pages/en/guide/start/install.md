<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# Install

Currently supported: deepin v23, UOS 1070.

## Operating system

- deepin v23

```bash
sudo apt install linglong-builder
```

- UOS 1070

add Linglong repository source

```bash
echo "deb [trusted=yes] https://ci.deepin.com/repo/deepin/deepin-community/linglong-repo/ unstable main" | sudo tee -a /etc/apt/sources.list
```

```bash
sudo apt update
sudo apt install linglong-builder
```
