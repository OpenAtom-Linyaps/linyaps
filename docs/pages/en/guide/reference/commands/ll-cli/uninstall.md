% ll-cli-uninstall 1

## NAME

ll\-cli\-uninstall - Uninstall applications or runtimes

## SYNOPSIS

**ll-cli uninstall** [*options*] _app_

## DESCRIPTION

The `ll-cli uninstall` command can uninstall Linyaps applications. This command is used to remove installed applications from the system.

## OPTIONS

**-h, --help**
: Print help information and exit

**--help-all**
: Expand all help

**--module** _MODULE_
: Uninstall specified module

## POSITIONAL ARGUMENTS

**APP** _TEXT_ _REQUIRED_
: Specify application name

## EXAMPLES

Use the `ll-cli uninstall` command to uninstall Linyaps applications:

```bash
ll-cli uninstall org.deepin.calculator
```

Output as follows:

```text
Uninstall main:org.deepin.calculator/5.7.21.4/x86_64 success:100%
```

After the command executes successfully, the Linyaps application will be removed from the system.

## SEE ALSO

**[ll-cli(1)](./ll-cli.md)**, **[ll-cli-install(1)](./install.md)**, **[ll-cli-list(1)](./list.md)**

## HISTORY

Developed in 2023 by UnionTech Software Technology Co., Ltd.
