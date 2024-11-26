<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# List Installed Apps

Use `ll-cli list` to view the installed linyaps apps.

View the help information for the `ll-cli list` command:

```bash
ll-cli list --help
```

Here is the output:

```text
List installed applications or runtimes
Usage: ll-cli list [OPTIONS]

Example:
# show installed application(s)
ll-cli list
# show installed runtime(s)
ll-cli list --type=runtime
# show the latest version list of the currently installed application(s)
ll-cli list --upgradable

Options:
  -h,--help                   Print this help message and exit
  --help-all                  Expand all help
  --type TYPE [app]           Filter result with specify type. One of "runtime", "app" or "all"
  --upgradable                Show the list of latest version of the currently installed applications, it only works for app

If you found any problems during use,
You can report bugs to the linyaps team under this project: https://github.com/OpenAtom-Linyaps/linyaps/issues
```

To view the installed apps, run `ll-cli list`:

```bash
ll-cli list
```

Here is the output:

```text
id                               name                             version         arch        channel         module      description
org.dde.calendar                 dde-calendar                     5.14.5.0        x86_64      main            binary      calendar for deepin os.
org.deepin.browser               deepin-browser                   6.5.5.4         x86_64      main            binary      browser for deepin os.
```
To view the installed runtime, run `ll-cli list --type=runtime`:

```bash
ll-cli list --type=runtime
```

Here is the output:

```text
id                               name                             version         arch        channel         module      description
org.deepin.Runtime               deepin runtime                   20.0.0.9        x86_64      main            runtime     Qt is a cross-platform C++ application framework. Qt'...
org.deepin.base                  deepin-foundation                23.1.0.2        x86_64      main            binary      deepin base environment.
org.deepin.Runtime               deepin runtime                   23.0.1.2        x86_64      main            runtime     Qt is a cross-platform C++ application framework. Qt'...
org.deepin.foundation            deepin-foundation                23.0.0.27       x86_64      main            runtime     deepin base environment.
```

To view the list of latest version of the currently installed applications, run `ll-cli list --upgradable`:

Here is the output:

```text
id                               installed       new
com.qq.wemeet.linyaps            3.19.2.402      3.19.2.403
org.dde.calendar                 5.14.5.0        5.14.5.1
```
