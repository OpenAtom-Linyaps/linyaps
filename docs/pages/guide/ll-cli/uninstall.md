<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# 卸载应用

`ll-cli uninstall`命令可以卸载玲珑应用。

查看`ll-cli uninstall`命令的帮助信息：

```bash
ll-cli uninstall --help
```

`ll-cli uninstall`命令的帮助信息如下：

```text
卸载应用程序或运行时
用法: ll-cli uninstall [选项] 应用

Positionals:
  APP TEXT REQUIRED           指定应用程序名

Options:
  -h,--help                   打印帮助信息并退出
  --help-all                  展开所有帮助
  --module MODULE             卸载指定模块


如果在使用过程中遇到任何问题，
您可以通过此项目向如意玲珑项目团队报告错误：https://github.com/OpenAtom-Linyaps/linyaps/issues
```

使用`ll-cli uninstall`命令卸载玲珑应用：

```bash
ll-cli uninstall <org.deepin.calculator>
```

`ll-cli uninstall org.deepin.calculator`命令输出如下：

```text
Uninstall main:org.deepin.calculator/5.7.21.4/x86_64 success:100%
```

该命令执行成功后，该玲珑应用将从系统中被卸载掉。
