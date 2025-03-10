<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# 构建应用

`ll-builder build`命令用来构建如意玲珑应用。

查看 `ll-builder build`命令的帮助信息：

```bash
ll-builder build --help
```

`ll-builder build`命令的帮助信息如下：

```text
构建如意玲珑项目
用法: ll-builder build [选项] [命令...]

Positionals:
  COMMAND TEXT ...            进入容器执行命令而不是构建应用

Options:
  -h,--help                   打印帮助信息并退出
  --help-all                  展开所有帮助
  --file FILE:FILE [./linglong.yaml]
                              linglong.yaml的文件路径
  --arch ARCH                 设置构建架构
  --offline                   仅使用本地文件。这意味着将设置--skip-fetch-source和--skip-pull-depend
  --skip-fetch-source         跳过获取源代码
  --skip-pull-depend          跳过拉取依赖项
  --skip-run-container        跳过运行容器
  --skip-commit-output        跳过提交构建输出
  --skip-output-check         跳过输出检查
  --skip-strip-symbols        跳过剥离调试符号
```

`ll-builder build` 命令可以通过以下两种方式运行：
1. 项目根目录，即 `linglong.yaml` 文件所在目录。
2. 使用 `--file` 参数指定 linglong.yaml 文件路径。

以如意玲珑项目 `org.deepin.demo`为例，构建如意玲珑应用主要步骤如下：

进入到 `org.deepin.demo`项目工程目录：

```bash
cd org.deepin.demo
```

执行 `ll-builder build`命令将开始构建如意玲珑应用:

```bash
ll-builder build
```

构建完成后，构建内容将自动提交到本地 `ostree`缓存中。导出构建内容见 `ll-builder export`。

使用 `--exec`参数可在构建脚本执行前进入如意玲珑容器：

```bash
ll-builder build --exec /bin/bash
```

进入容器后，可执行 `shell`命令，如 `ps`、`ls` 等。

如意玲珑应用 `debug`版本更多调试信息请参考：[DEBUG](../debug/debug.md)。
