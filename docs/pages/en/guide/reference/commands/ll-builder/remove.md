% ll-builder-remove 1

## NAME

ll-builder-remove - Remove built applications

## SYNOPSIS

**ll-builder remove** [*options*] _package_

## DESCRIPTION

The `ll-builder remove` command is used to remove built applications.

## OPTIONS

**-h, --help**
: Print help information and exit

**--help-all**
: Expand all help

**--no-clean-objects**
: Do not clean object files before removing application

**package** (required)
: Application package name to be removed

## EXAMPLES

Remove specified application:

```bash
ll-builder remove org.deepin.demo
```

## SEE ALSO

**[ll-builder(1)](./ll-builder.md)**, **[ll-builder-list(1)](list.md)**

## HISTORY

Developed in 2023 by UnionTech Software Technology Co., Ltd.
