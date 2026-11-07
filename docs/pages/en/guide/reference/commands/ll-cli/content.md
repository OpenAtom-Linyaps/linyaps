% ll-cli-content 1

## NAME

ll\-cli\-content - Display application exported files

## SYNOPSIS

**ll-cli content** [*options*] _app_

## DESCRIPTION

Use `ll-cli content` to display application exported files. This command is used to display the file paths exported by installed applications, including desktop files, metadata files, etc.

## OPTIONS

**-h, --help**
: Print help information and exit

**--help-all**
: Expand all help

## POSITIONAL ARGUMENTS

**APP** _TEXT_ _REQUIRED_
: Specify installed application name

## EXAMPLES

Use `ll-cli content org.dde.calendar` to display the exported files of the calendar application:

```bash
ll-cli content org.dde.calendar
```

`ll-cli content org.dde.calendar` output as follows:

```text
/var/lib/linglong/entries/share/applications
/var/lib/linglong/entries/share/applications/dde-calendar.desktop
/var/lib/linglong/entries/share/metainfo
/var/lib/linglong/entries/share/metainfo/org.deepin.calendar.metainfo.xml
```

## SEE ALSO

**[ll-cli(1)](./ll-cli.md)**, **[ll-cli-info(1)](info.md)**, **[ll-cli-list(1)](list.md)**

## HISTORY

Developed in 2023 by UnionTech Software Technology Co., Ltd.
