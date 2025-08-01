<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# 安装应用

`ll-cli install`命令用来安装如意玲珑应用。

查看 `ll-cli install`命令的帮助信息：

```bash
ll-cli install --help
```

`ll-cli install`命令的帮助信息如下：

```text
安装应用程序或运行时
用法: ll-cli install [选项] 应用程序

示例:
# 通过应用名安装应用程序
ll-cli install org.deepin.demo
# 通过如意玲珑.layer文件安装应用程序
ll-cli install demo_0.0.0.1_x86_64_binary.layer
# 通过通过如意玲珑.uab文件安装应用程序
ll-cli install demo_x86_64_0.0.0.1_main.uab
# 安装应用的指定模块
ll-cli install org.deepin.demo --module=binary
# 安装应用的指定版本
ll-cli install org.deepin.demo/0.0.0.1
# 通过特定标识安装应用程序
ll-cli install stable:org.deepin.demo/0.0.0.1/x86_64


Positionals:
  APP TEXT REQUIRED           指定应用名，也可以是 .uab 或 .layer 文件

Options:
  -h,--help                   打印帮助信息并退出
  --help-all                  展开所有帮助
  --module MODULE             安装指定模块
  --repo REPO                 从指定的仓库安装
  --force                     强制安装指定版本的应用程序
  -y                          自动对所有问题回答是

如果在使用过程中遇到任何问题，
您可以通过此项目向如意玲珑项目团队报告错误：https://github.com/OpenAtom-Linyaps/linyaps/issues
```

运行 `ll-cli install`命令安装如意玲珑应用:

```bash
ll-cli install <org.deepin.calculator>
```

`ll-cli install`命令需要输入应用完整的 `appid`，若仓库有多个版本则会默认安装最高版本。

安装指定版本需在 `appid`后附加对应版本号:

```bash
ll-cli install <org.deepin.calculator/5.1.2>
```

`ll-cli install org.deepin.calculator`输出如下：

```text
Install main:org.deepin.calculator/5.7.21.4/x86_64 success:100%
```

应用安装完成后，客户端会显示安装结果信息。

我们在使用 `ll-builder export` 命令导出的 layer 或者 uab 文件，可以使用 `ll-cli install` 进行安装。

`.layer` 文件

```bash
ll-cli install ./com.baidu.baidunetdisk_4.17.7.0_x86_64_runtime.layer
```

`.uab` 文件
uab文件有以下两种安装方式

- 通过 `ll-cli install` 进行安装

```bash
ll-cli install com.baidu.baidunetdisk_x86_64_4.17.7.0_main.uab
```

- 通过直接运行`.uab`的方式进行安装

```bash
./com.baidu.baidunetdisk_x86_64_4.17.7.0_main.uab
```

可以使用 `ll-cli list | grep com.baidu.baidunetdisk` 命令来查看是否安装成功。

使用下面的命令运行应用。

```bash
ll-cli run com.baidu.baidunetdisk
```
