<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# 上架应用到商店

## 主要流程

- 创建一个玲珑工程，使用ll-builder在本地构建出玲珑应用。

- 将玲珑工程以pull request（PR）方式提交到[https://github.com/linuxdeepin/linglong-hub](https://github.com/linuxdeepin/linglong-hub)项目。

- 项目维护者审核内容通过后，将触发构建流程，构建成功后内容将被提交到玲珑测试仓库。

- 应用通过测试后将会合并PR，并将应用上架到[玲珑网页商店](https://store.linglong.dev)。
