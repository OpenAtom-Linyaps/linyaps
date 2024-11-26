<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# Display App exported files

Use `ll-cli content` to display the exported files.

View the help information for the `ll-cli content` command:

```bash
ll-cli content --help
```

Here is the output:

```text
Display the exported files of installed application
Usage: ll-cli content [OPTIONS] APP

Positionals:
  APP TEXT REQUIRED           Specify the installed application ID

Options:
  -h,--help                   Print this help message and exit
  --help-all                  Expand all help

If you found any problems during use,
You can report bugs to the linyaps team under this project: https://github.com/OpenAtom-Linyaps/linyaps/issues
```

Use `ll-cli content org.dde.calendar` to display the exported files of org.dde.calendar.

Here is the output:

```text
/var/lib/linglong/entries/share/applications
/var/lib/linglong/entries/share/applications/dde-calendar.desktop
/var/lib/linglong/entries/share/metainfo
/var/lib/linglong/entries/share/metainfo/org.deepin.calendar.metainfo.xml
```
