<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# 构建应用

`ll-builder build`命令用来构建玲珑应用。

查看 `ll-builder build`命令的帮助信息：

```bash
ll-builder build --help
```

`ll-builder build`命令的帮助信息如下：

```text
Usage: ll-builder [options] build
linglong build command tools
Examples:
ll-builder build -v
ll-builder build -v -- bash -c "echo hello"

Options:
  -v, --verbose         show detail log (deprecated, use QT_LOGGING_RULES)
  -h, --help            Displays help on commandline options.
  --help-all            Displays help including Qt specific options.
  --exec <command>      run exec than build script
  --offline             only use local files. This implies --skip-fetch-source
                        and --skip-pull-depend
  --skip-fetch-source   skip fetch sources
  --skip-pull-depend    skip pull dependency
  --skip-run-container  skip run container. This implies skip-commit-output
  --skip-commit-output  skip commit build output
  --arch <arch>         set the build arch

Arguments:
  build                 build project
```

`ll-builder build`命令必须运行在工程的根目录，即 `linglong.yaml`文件所在位置。

以玲珑项目 `org.deepin.demo`为例，构建玲珑应用主要步骤如下：

进入到 `org.deepin.demo`项目工程目录：

```bash
cd org.deepin.demo
```

执行 `ll-builder build`命令将开始构建玲珑应用:

```bash
ll-builder build
```

构建完成后，构建内容将自动提交到本地 `ostree`缓存中。导出构建内容见 `ll-builder export`。

使用 `--exec`参数可在构建脚本执行前进入玲珑容器：

```bash
ll-builder build --exec /bin/bash
```

进入容器后，可执行 `shell`命令，如 `ps`、`ls` 等。

玲珑应用 `debug`版本更多调试信息请参考：[DEBUG](../debug/debug.md)。
