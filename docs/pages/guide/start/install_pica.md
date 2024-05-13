<!--
SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# 安装 pica 工具

## 系统

- deepin v23
  ```bash
  sudo apt install linglong-pica
  ```
- UOS 1070

添加玲珑仓库源，如果前面已经添加了仓库源，就不需要再添加了。

```bash
echo "deb [trusted=yes] https://ci.deepin.com/repo/deepin/deepin-community/linglong-repo/ unstable main" | sudo tee -a /etc/apt/sources.list
```

```bash
sudo apt update
sudo apt install linglong-pica
```
