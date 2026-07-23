% ll-cli-uninstall 1

## NAME

ll\-cli\-uninstall - Uninstall applications or runtimes

## SYNOPSIS

**ll-cli uninstall** [*options*] _app_

## DESCRIPTION

The `ll-cli uninstall` command removes installed Linyaps applications. Base and Runtime packages are protected from uninstallation by default; use `ll-cli prune` to remove unused Base and Runtime packages.

## OPTIONS

**-h, --help**
: Print help information and exit

**--help-all**
: Expand all help

**--module** _MODULE_
: Uninstall specified module

**--force**
: Force the uninstallation of a Base or Runtime

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

Force-uninstall a specific Runtime version:

```bash
ll-cli uninstall main:org.deepin.runtime.dtk/23.1.0/x86_64 --force
```

Forced uninstallation can break applications that still depend on the Runtime. Use `ll-cli prune` instead when cleaning up a Runtime that no application uses.

## SEE ALSO

**[ll-cli(1)](./ll-cli.md)**, **[ll-cli-install(1)](./install.md)**, **[ll-cli-list(1)](./list.md)**, **[ll-cli-prune(1)](./prune.md)**

## HISTORY

Developed in 2023 by UnionTech Software Technology Co., Ltd.
