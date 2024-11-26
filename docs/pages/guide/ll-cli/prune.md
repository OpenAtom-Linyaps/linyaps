<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# 移除未使用的最小系统或运行时

使用`ll-cli prune`移除未使用的最小系统或运行时。

查看`ll-cli prune`命令的帮助信息：

```bash
ll-cli prune --help
```

`ll-cli prune`命令的帮助信息如下：

```text
移除未使用的最小系统或运行时
用法: ll-cli prune [选项]

Options:
  -h,--help                   打印帮助信息并退出
  --help-all                  展开所有帮助

如果在使用过程中遇到任何问题，
您可以通过此项目向如意玲珑项目团队报告错误：https://github.com/OpenAtom-Linyaps/linyaps/issues
```

使用`ll-cli prune`移除未使用的最小系统或运行时。

`ll-cli prune`输出如下:

```text
Unused base or runtime:
main:org.deepin.Runtime/23.0.1.2/x86_64
main:org.deepin.foundation/20.0.0.27/x86_64
main:org.deepin.Runtime/23.0.1.0/x86_64
main:org.deepin.foundation/23.0.0.27/x86_64
main:org.deepin.Runtime/20.0.0.8/x86_64
5 unused base or runtime have been removed.
```
