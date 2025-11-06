% ll-cli-upgrade 1

## NAME

ll\-cli\-upgrade - Upgrade applications or runtimes

## SYNOPSIS

**ll-cli upgrade** [*options*] [*app*]

## DESCRIPTION

The `ll-cli upgrade` command can update Linyaps applications. This command is used to upgrade installed applications or runtimes to the latest version in the remote repository.

## OPTIONS

**-h, --help**
: Print help information and exit

**--help-all**
: Expand all help

## POSITIONAL ARGUMENTS

**APP** _TEXT_
: Specify application name. If not specified, all upgradeable applications will be upgraded

## EXAMPLES

Use `ll-cli upgrade` to upgrade all installed applications to the latest version:

```bash
ll-cli upgrade
```

Output as follows:

```text
Upgrade main:org.dde.calendar/5.14.5.0/x86_64 to main:org.dde.calendar/5.14.5.1/x86_64 success:100%
Upgrade main:org.deepin.mail/6.4.10.0/x86_64 to main:org.deepin.mail/6.4.10.1/x86_64 success:100%
Please restart the application after saving the data to experience the new version.
```

Use `ll-cli upgrade org.deepin.calculator` to upgrade the org.deepin.calculator application to the latest version in the remote repository:

```bash
ll-cli upgrade org.deepin.calculator
```

Output as follows:

```text
Upgrade main:org.deepin.calculator/5.7.21.3/x86_64 to main:org.deepin.calculator/5.7.21.4/x86_64 success:100%
Please restart the application after saving the data to experience the new version.
```

## SEE ALSO

**[ll-cli(1)](./ll-cli.md)**, **[ll-cli-install(1)](./install.md)**, **[ll-cli-list(1)](./list.md)**

## HISTORY

Developed in 2023 by UnionTech Software Technology Co., Ltd.
