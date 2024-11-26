<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# 更新应用

`ll-cli upgrade`命令可以更新玲珑应用。

查看 `ll-cli upgrade`命令的帮助信息：

```bash
ll-cli upgrade --help
```

`ll-cli upgrade`命令的帮助信息如下：

```text
升级应用程序或运行时
用法: ll-cli upgrade [选项] [应用]

Positionals:
  APP TEXT                    指定应用程序名。如果未指定，将升级所有可升级的应用程序

Options:
  -h,--help                   打印帮助信息并退出
  --help-all                  展开所有帮助

如果在使用过程中遇到任何问题，
您可以通过此项目向如意玲珑项目团队报告错误：https://github.com/OpenAtom-Linyaps/linyaps/issues
```

使用 `ll-cli upgrade`将所有已安装的应用程序升级到最新版本

`ll-cli upgrade`命令输出如下：

```text
Upgrade main:org.dde.calendar/5.14.5.0/x86_64 to main:org.dde.calendar/5.14.5.1/x86_64 success:100%
Upgrade main:org.deepin.mail/6.4.10.0/x86_64 to main:org.deepin.mail/6.4.10.1/x86_64 success:100%
Please restart the application after saving the data to experience the new version.
```

使用 `ll-cli upgrade org.deepin.calculator`将org.deepin.calculator应用程序升级到远程存储库中的最新版本，例如：

```bash
ll-cli upgrade org.deepin.calculator
```

`ll-cli upgrade org.deepin.calculator`命令输出如下：

```text
Upgrade main:org.deepin.calculator/5.7.21.3/x86_64 to main:org.deepin.calculator/5.7.21.4/x86_64 success:100%
Please restart the application after saving the data to experience the new version.
```
