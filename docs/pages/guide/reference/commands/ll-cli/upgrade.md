% ll-cli-upgrade 1

## NAME

ll\-cli\-upgrade - 升级应用程序或运行时

## SYNOPSIS

**ll-cli upgrade** [*options*] [*app*]

## DESCRIPTION

`ll-cli upgrade` 命令可以更新玲珑应用。该命令用于将已安装的应用程序或运行时升级到远程仓库中的最新版本。

## OPTIONS

**-h, --help**
: 打印帮助信息并退出

**--help-all**
: 展开所有帮助

## POSITIONAL ARGUMENTS

**APP** _TEXT_
: 指定应用程序名。如果未指定，将升级所有可升级的应用程序

## EXAMPLES

使用 `ll-cli upgrade` 将所有已安装的应用程序升级到最新版本：

```bash
ll-cli upgrade
```

输出如下：

```text
Upgrade main:org.dde.calendar/5.14.5.0/x86_64 to main:org.dde.calendar/5.14.5.1/x86_64 success:100%
Upgrade main:org.deepin.mail/6.4.10.0/x86_64 to main:org.deepin.mail/6.4.10.1/x86_64 success:100%
Please restart the application after saving the data to experience the new version.
```

使用 `ll-cli upgrade org.deepin.calculator` 将 org.deepin.calculator 应用程序升级到远程存储库中的最新版本：

```bash
ll-cli upgrade org.deepin.calculator
```

输出如下：

```text
Upgrade main:org.deepin.calculator/5.7.21.3/x86_64 to main:org.deepin.calculator/5.7.21.4/x86_64 success:100%
Please restart the application after saving the data to experience the new version.
```

## SEE ALSO

**[ll-cli(1)](./ll-cli.md)**, **[ll-cli-install(1)](./install.md)**, **[ll-cli-list(1)](./list.md)**

## HISTORY

2023年，由 UnionTech Software Technology Co., Ltd. 开发
