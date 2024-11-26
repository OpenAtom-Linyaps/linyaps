<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# 推送uab到远程仓库

`ll-builder push`命令用来将如意玲珑软件包推送至如意玲珑远程仓库。

查看`ll-builder push`命令的帮助信息：

```bash
ll-builder push --help
```

`ll-builder push`命令的帮助信息如下：

```text
推送如意玲珑应用到远程仓库
用法: ll-builder push [选项]

Options:
  -h,--help                   打印帮助信息并退出
  --help-all                  展开所有帮助
  --file FILE:FILE [./linglong.yaml]
                              linglong.yaml的文件路径
  --repo-url URL              远程仓库URL
  --repo-name NAME            远程仓库名
  --module TEXT               推送单个模块
```

`ll-builder push`命令根据输入的文件路径，读取`bundle`格式软件包内容，将本地软件数据及`bundle`格式软件包传输到服务端。

```bash
ll-builder push <org.deepin.demo-1.0.0_x86_64.uab>
```
