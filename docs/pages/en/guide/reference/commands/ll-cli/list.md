% ll-cli-list 1

## NAME

ll\-cli\-list - List installed applications or runtimes

## SYNOPSIS

**ll-cli list** [*options*]

## DESCRIPTION

The `ll-cli list` command can view installed Linyaps applications. This command is used to display detailed information about installed applications, runtimes, and base environments in the system.

## OPTIONS

**-h, --help**
: Print help information and exit

**--help-all**
: Expand all help

**--type** _TYPE_ [*all*]
: Filter results using specified type. One of "runtime", "base", "app", or "all"

**--upgradable**
: Display the latest version list of currently installed applications, applicable only to applications

## EXAMPLES

View installed applications, run the `ll-cli list` command:

```bash
ll-cli list
```

`ll-cli list` output as follows:

```text
id                               name                             version         arch        channel         module      description
org.dde.calendar                 dde-calendar                     5.14.5.0        x86_64      main            binary      calendar for deepin os.
org.deepin.browser               deepin-browser                   6.5.5.4         x86_64      main            binary      browser for deepin os.
```

View installed runtime environments, run the `ll-cli list --type=runtime` command:

```bash
ll-cli list --type=runtime
```

Output as follows:

```text
id                               name                             version         arch        channel         module      description
org.deepin.Runtime               deepin runtime                   20.0.0.9        x86_64      main            runtime     Qt is a cross-platform C++ application framework. Qt'...
org.deepin.base                  deepin-foundation                23.1.0.2        x86_64      main            binary      deepin base environment.
org.deepin.Runtime               deepin runtime                   23.0.1.2        x86_64      main            runtime     Qt is a cross-platform C++ application framework. Qt'...
org.deepin.foundation            deepin-foundation                23.0.0.27       x86_64      main            runtime     deepin base environment.
```

Display the latest version list of currently installed applications, applicable only to applications. Run `ll-cli list --upgradable`:

```bash
ll-cli list --upgradable
```

`ll-cli list --upgradable` output as follows:

```text
id                               installed       new
com.qq.wemeet.linyaps            3.19.2.402      3.19.2.403
org.dde.calendar                 5.14.5.0        5.14.5.1
```

## SEE ALSO

**[ll-cli(1)](./ll-cli.md)**, **[ll-cli-info(1)](info.md)**, **[ll-cli-search(1)](search.md)**

## HISTORY

Developed in 2023 by UnionTech Software Technology Co., Ltd.
