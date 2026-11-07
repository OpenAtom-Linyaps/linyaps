% ll-cli-info 1

## NAME

ll\-cli\-info - Display information about installed applications or runtimes

## SYNOPSIS

**ll-cli info** [*options*] _app_

## DESCRIPTION

Use `ll-cli info` to display information about installed applications or runtimes. This command shows detailed information about applications or runtimes in JSON format, including architecture, base environment, runtime, description, etc.

## OPTIONS

**-h, --help**
: Print help information and exit

**--help-all**
: Expand all help

## POSITIONAL ARGUMENTS

**APP** _TEXT_ _REQUIRED_
: Specify application name, can also be a Linyaps.layer file

## EXAMPLES

Use `ll-cli info` to display information about installed applications or runtimes:

```bash
ll-cli info org.dde.calendar
```

`ll-cli info org.dde.calendar` output as follows:

```text
{
    "arch": [
        "x86_64"
    ],
    "base": "main:org.deepin.base/23.1.0/x86_64",
    "channel": "main",
    "command": [
        "dde-calendar"
    ],
    "description": "calendar for deepin os.\n",
    "id": "org.dde.calendar",
    "kind": "app",
    "module": "binary",
    "name": "dde-calendar",
    "runtime": "main:org.deepin.runtime.dtk/23.1.0/x86_64",
    "schema_version": "1.0",
    "size": 13483249,
    "version": "5.14.5.1"
}
```

## SEE ALSO

**[ll-cli(1)](./ll-cli.md)**, **[ll-cli-list(1)](list.md)**, **[ll-cli-content(1)](content.md)**

## HISTORY

Developed in 2023 by UnionTech Software Technology Co., Ltd.
