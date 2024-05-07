<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# ll-cli Introduction

`ll-cli` is the command line tool of Linglong, used to install, uninstall, check, run, close, debug, and update Linglong applications.

View the help information for the `ll-cli` command:

```bash
ll-cli --help
```

Here is the output:

```text
Usage: ll-cli [options] subcommand [sub-option]

Options:
    -h --help                 Show this screen.
    --version                 Show version.
    --json                    Use json to output command result.
    --no-dbus                 Use peer to peer DBus, this is used only in case that DBus daemon is not available.
    --no-dbus-proxy           Do not enable linglong-dbus-proxy.
    --dbus-proxy-cfg=PATH     Path of config of linglong-dbus-proxy.
    --file=FILE               you can refer to https://linglong.dev/guide/ll-cli/run.html to use this parameter.
    --url=URL                 you can refer to https://linglong.dev/guide/ll-cli/run.html to use this parameter.
    --working-directory=PATH  Specify working directory.
    --type=TYPE               Filter result with tiers type. One of "lib", "app" or "dev". [default: app]
    --state=STATE             Filter result with the tiers install state. Should be "local" or "remote". [default: local]
    --prune                   Remove application data if the tier is an application and all version of that application has been removed.

Subcommands:
    run        Run an application.
    ps         List all pagodas.
    exec       Execute command in a pagoda.
    enter      Enter a pagoda.
    kill       Stop applications and remove the pagoda.
    install    Install tier(s).
    uninstall  Uninstall tier(s).
    upgrade    Upgrade tier(s).
    search     Search for tiers.
    list       List known tiers.
    repo       Display or modify information of the repository currently using.
    info       Display the information of layer
```
