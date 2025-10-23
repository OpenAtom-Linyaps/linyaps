% ll-cli-prune 1

## NAME

ll\-cli\-prune - Remove unused minimal system or runtime

## SYNOPSIS

**ll-cli prune** [*options*]

## DESCRIPTION

Use `ll-cli prune` to remove unused minimal systems or runtimes. This command is used to clean up base environments or runtime components in the system that are no longer used by any applications, freeing up disk space.

## OPTIONS

**-h, --help**
: Print help information and exit

**--help-all**
: Expand all help

## EXAMPLES

Use `ll-cli prune` to remove unused minimal systems or runtimes:

```bash
ll-cli prune
```

`ll-cli prune` output as follows:

```text
Unused base or runtime:
main:org.deepin.Runtime/23.0.1.2/x86_64
main:org.deepin.foundation/20.0.0.27/x86_64
main:org.deepin.Runtime/23.0.1.0/x86_64
main:org.deepin.foundation/23.0.0.27/x86_64
main:org.deepin.Runtime/20.0.0.8/x86_64
5 unused base or runtime have been removed.
```

## SEE ALSO

**[ll-cli(1)](./ll-cli.md)**, **[ll-cli-list(1)](./list.md)**, **[ll-cli-uninstall(1)](./uninstall.md)**

## HISTORY

Developed in 2023 by UnionTech Software Technology Co., Ltd.
