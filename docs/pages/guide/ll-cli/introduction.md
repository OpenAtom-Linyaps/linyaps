<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# ll-cli简介

`ll-cli`是一个包管理器前端，用于管理如意玲珑应用的安装、卸载、查看、启动、关闭、调试、更新等操作。

查看 `ll-cli`命令的帮助信息：

```bash
ll-cli --help
```

`ll-cli`命令的帮助信息如下：

```text
如意玲珑 CLI
一个用于运行应用程序和管理应用程序和运行时的命令行工具

用法: ll-cli [选项] [子命令]

Options:
  -h,--help                   打印帮助信息并退出
  --help-all                  展开所有帮助
  --version                   显示版本
  --json                      使用json格式输出结果


管理正在运行的应用程序:
  run                         运行应用程序
  ps                          列出正在运行的应用程序
  enter                       进入应用程序正在运行的命名空间
  kill                        停止运行的应用程序
  prune                       移除未使用的最小系统或运行时

管理已安装的应用程序和运行时:
  install                     安装应用程序或运行时
  uninstall                   卸载应用程序或运行时
  upgrade                     升级应用程序或运行时
  list                        列出已安装的应用程序或运行时
  info                        显示已安装的应用程序或运行时的信息
  content                     显示已安装应用导出的文件

查找应用程序和运行时:
  search                      从远程仓库中搜索包含指定关键词的应用程序/运行时

管理仓库:
  repo                        显示或修改当前使用的仓库信息

如果在使用过程中遇到任何问题，
您可以通过此项目向如意玲珑项目团队报告错误：https://github.com/OpenAtom-Linyaps/linyaps/issues
```
