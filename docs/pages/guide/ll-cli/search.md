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
# 按照名称搜索远程的 application(s), base(s) 或 runtime(s)
ll-cli search org.deepin.demo
# 按名称搜索远程 runtime
ll-cli search org.deepin.base --type=runtime
#  搜索远程所有软件包
ll-cli search .
# 搜索远程所有的base
ll-cli search . --type=base
# 搜索远程所有的runtime
ll-cli search . --type=runtime

Positionals:
  KEYWORDS TEXT REQUIRED      指定搜索关键词

Options:
  -h,--help                   打印帮助信息并退出
  --help-all                  展开所有帮助
  --type TYPE [all]           按指定类型过滤结果。可选类型："runtime"、"base"、"app"或 "all"。
  --repo REPO                 指定仓库
  --dev                       结果包括应用调试包
  --show-all-version          显示应用，base或runtime的所有版本

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

如果需要查找远程所有的base可以使用以下命令：

```bash
ll-cli search . --type=base
```

如果需要查找远程所有的runtime可以使用以下命令：

```bash
ll-cli search . --type=runtime
```
