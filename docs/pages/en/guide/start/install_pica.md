<!--
SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# Install the Pica tool

## Operating system

- deepin v23
  ```bash
  sudo apt install linglong-pica
  ```
- UOS 1070

add Linglong repository source，If the repository source has already been added previously, there is no need to add it again.

```bash
echo "deb [trusted=yes] https://ci.deepin.com/repo/deepin/deepin-community/linglong-repo/ unstable main" | sudo tee -a /etc/apt/sources.list
```

```bash
sudo apt update
sudo apt install linglong-pica
```
