% ll-cli-uninstall 1

## NAME

ll\-cli\-uninstall - 卸载应用程序或运行时

## SYNOPSIS

**ll-cli uninstall** [*options*] _app_

## DESCRIPTION

`ll-cli uninstall` 命令可以卸载已安装的玲珑应用。Base 和 Runtime 默认受卸载保护，未使用的 Base 和 Runtime 应使用 `ll-cli prune` 清理。

## OPTIONS

**-h, --help**
: 打印帮助信息并退出

**--help-all**
: 展开所有帮助

**--module** _MODULE_
: 卸载指定模块

**--force**
: 强制卸载基础环境（Base）或运行时（Runtime）

## POSITIONAL ARGUMENTS

**APP** _TEXT_ _REQUIRED_
: 指定应用程序名

## EXAMPLES

使用 `ll-cli uninstall` 命令卸载玲珑应用：

```bash
ll-cli uninstall org.deepin.calculator
```

输出如下：

```text
Uninstall main:org.deepin.calculator/5.7.21.4/x86_64 success:100%
```

该命令执行成功后，该玲珑应用将从系统中被卸载掉。

强制卸载指定版本的 Runtime：

```bash
ll-cli uninstall main:org.deepin.runtime.dtk/23.1.0/x86_64 --force
```

强制卸载可能破坏仍依赖该 Runtime 的应用；清理未被任何应用使用的 Runtime 时，应改用 `ll-cli prune`。

## SEE ALSO

**[ll-cli(1)](./ll-cli.md)**, **[ll-cli-install(1)](./install.md)**, **[ll-cli-list(1)](./list.md)**, **[ll-cli-prune(1)](./prune.md)**

## HISTORY

2023年，由 UnionTech Software Technology Co., Ltd. 开发
