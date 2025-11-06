% ll-cli-content 1

## NAME

ll\-cli\-content - 显示应用导出文件

## SYNOPSIS

**ll-cli content** [*options*] _app_

## DESCRIPTION

使用 `ll-cli content` 显示应用导出文件。该命令用于显示已安装应用程序导出的文件路径，包括桌面文件、元信息文件等。

## OPTIONS

**-h, --help**
: 打印帮助信息并退出

**--help-all**
: 展开所有帮助

## POSITIONAL ARGUMENTS

**APP** _TEXT_ _REQUIRED_
: 指定已安装应用程序名

## EXAMPLES

使用 `ll-cli content org.dde.calendar` 显示日历应用的导出的文件：

```bash
ll-cli content org.dde.calendar
```

`ll-cli content org.dde.calendar` 输出如下:

```text
/var/lib/linglong/entries/share/applications
/var/lib/linglong/entries/share/applications/dde-calendar.desktop
/var/lib/linglong/entries/share/metainfo
/var/lib/linglong/entries/share/metainfo/org.deepin.calendar.metainfo.xml
```

## SEE ALSO

**[ll-cli(1)](./ll-cli.md)**, **[ll-cli-info(1)](info.md)**, **[ll-cli-list(1)](list.md)**

## HISTORY

2023年，由 UnionTech Software Technology Co., Ltd. 开发
