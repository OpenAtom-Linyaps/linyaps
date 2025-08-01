<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# 如意玲珑构建工具介绍

## ll-builder简介

`ll-builder`是为应用开发者提供的一款构建如意玲珑应用工具。

主要功能如下：

- 支持在独立容器内构建。

<!-- - 定义了一套版本管理系统。 -->

- 提供 DTK 开发套件。

<!-- - 包含完整推送发布流程。 -->

查看 `ll-builder`命令的帮助信息：

```bash
ll-builder --help
```

ll-builder 命令的帮助信息如下：

```text
如意玲珑构建工具 CLI
一个用于构建如意玲珑应用的命令行工具

用法: ll-builder [选项] [子命令]

Options:
  -h,--help                   打印帮助信息并退出
  --help-all                  展开所有帮助
  --version                   显示版本

Subcommands:
  create                      创建如意玲珑构建模板
  build                       构建如意玲珑项目
  run                         运行构建好的应用程序
  list                        列出已构建的应用程序
  remove                      删除已构建的应用程序
  export                      导出如意玲珑layer或uab
  push                        推送如意玲珑应用到远程仓库
  import                      导入如意玲珑layer文件到构建仓库
  extract                     将如意玲珑layer文件解压到目录
  repo                        显示和管理仓库

如果在使用过程中遇到任何问题，
您可以通过此项目向如意玲珑项目团队报告错误：https://github.com/OpenAtom-Linyaps/linyaps/issues
```

## 第三方工具简介

[ll-killer](https://github.com/System233/ll-killer-go) 专为解决玲珑容器应用构建问题而设计的命令行工具

[linglong-tools](https://github.com/myml/linglong-tools) 一个玲珑的命令行辅助工具

[linglong-tools](https://github.com/System233/linglong-tools) 玲珑自动化deb转制工具 

[linglong-builder-action](https://github.com/myml/linglong-builder-action) github action构建玲珑包
