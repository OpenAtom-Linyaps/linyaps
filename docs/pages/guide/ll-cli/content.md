<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# 显示应用导出文件

使用 `ll-cli content` 显示应用导出文件。

查看`ll-cli content`命令的帮助信息：

```bash
ll-cli content --help
```

`ll-cli content`命令的帮助信息如下：

```text
显示已安装应用导出的文件
用法: ll-cli content [选项] 应用

Positionals:
  APP TEXT REQUIRED           指定已安装应用程序名

Options:
  -h,--help                   打印帮助信息并退出
  --help-all                  展开所有帮助

如果在使用过程中遇到任何问题，
您可以通过此项目向如意玲珑项目团队报告错误：https://github.com/OpenAtom-Linyaps/linyaps/issues
```

使用 `ll-cli content org.dde.calendar` 显示日历应用的导出的文件。

`ll-cli content org.dde.calendar`输出如下:

```text
/var/lib/linglong/entries/share/applications
/var/lib/linglong/entries/share/applications/dde-calendar.desktop
/var/lib/linglong/entries/share/metainfo
/var/lib/linglong/entries/share/metainfo/org.deepin.calendar.metainfo.xml
```
