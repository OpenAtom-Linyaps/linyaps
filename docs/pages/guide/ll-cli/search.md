<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# 从远程仓库查询应用

`ll-cli search`命令可以查询如意玲珑远程仓库中的应用信息。

查看 `ll-cli search`命令的帮助信息：

```bash
ll-cli search --help
```

`ll-cli search`命令的帮助信息如下：

```text
从远程仓库中搜索包含指定关键词的应用程序/运行时
用法: ll-cli search [选项] 关键词

示例:
# 通过应用名从远程仓库搜索应用程序
ll-cli search org.deepin.demo
# 从远程仓库里搜索指定类型的应用程序
ll-cli search org.deepin.base --type=runtime
# 从远程仓库搜索所有应用程序
ll-cli search .
# 从远程仓库搜索所有运行时
ll-cli search . --type=runtime

Positionals:
  KEYWORDS TEXT REQUIRED      指定搜索关键词

Options:
  -h,--help                   打印帮助信息并退出
  --help-all                  展开所有帮助
  --type TYPE [app]           使用指定类型过滤结果。“runtime”、“app”或“all”之一
  --dev                       搜索结果中包括应用调试包

如果在使用过程中遇到任何问题，
您可以通过此项目向如意玲珑项目团队报告错误：https://github.com/OpenAtom-Linyaps/linyaps/issues
```

通过 `ll-cli search`命令可以从远程 repo 中查找应用程序信息:

```bash
ll-cli search calculator
```

该命令将返回包含 calculator关键词的所有应用程序信息，包含完整的 `appid`、应用程序名称、版本、平台及应用描述信息。

`ll-cli search calculator`输出如下：

```text
appId                           name                            version         arch        channel         module      description
org.deepin.calculator           deepin-calculator               5.5.23          x86_64      linglong        runtime     Calculator for UOS
org.deepin.calculator           deepin-calculator               5.7.1           x86_64      linglong        runtime     Calculator for UOS

```

如果需要查找 Base 和 Runtime 可以使用以下命令：

```bash
ll-cli search . --type=runtime
```
