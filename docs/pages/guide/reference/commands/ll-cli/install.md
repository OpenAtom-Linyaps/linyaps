% ll-cli-install 1

## NAME

ll\-cli\-install - 安装应用程序或运行时

## SYNOPSIS

**ll-cli install** [*options*] _app_

## DESCRIPTION

`ll-cli install` 命令用来安装如意玲珑应用。该命令支持通过应用名、.layer 文件或 .uab 文件安装应用程序或运行时。

## OPTIONS

**-h, --help**
: 打印帮助信息并退出

**--help-all**
: 展开所有帮助

**--module** _MODULE_
: 安装指定模块

**--repo** _REPO_
: 从指定的仓库安装

**--force**
: 强制安装指定版本的应用程序

**-y**
: 自动对所有问题回答是

## POSITIONAL ARGUMENTS

**APP** _TEXT_ _REQUIRED_
: 指定应用名，也可以是 .uab 或 .layer 文件

## EXAMPLES

通过应用名安装应用程序：

```bash
ll-cli install org.deepin.demo
```

通过如意玲珑.layer文件安装应用程序：

```bash
ll-cli install demo_0.0.0.1_x86_64_binary.layer
```

通过如意玲珑.uab文件安装应用程序：

```bash
ll-cli install demo_x86_64_0.0.0.1_main.uab
```

安装应用的指定模块：

```bash
ll-cli install org.deepin.demo --module=binary
```

安装应用的指定版本：

```bash
ll-cli install org.deepin.demo/0.0.0.1
```

通过特定标识安装应用程序：

```bash
ll-cli install stable:org.deepin.demo/0.0.0.1/x86_64
```

## SEE ALSO

**[ll-cli(1)](./ll-cli.md)**, **[ll-cli-uninstall(1)](uninstall.md)**, **[ll-cli-upgrade(1)](upgrade.md)**, **[ll-cli-list(1)](list.md)**

## HISTORY

2023年，由 UnionTech Software Technology Co., Ltd. 开发
