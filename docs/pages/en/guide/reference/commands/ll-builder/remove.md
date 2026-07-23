% ll-builder-remove 1

## NAME

ll-builder-remove - Remove built applications

## SYNOPSIS

**ll-builder remove** [*options*] _ref_...

## DESCRIPTION

The `ll-builder remove` command removes one or more build results from the local build repository. Specify each result using the complete ref shown by `ll-builder list`.

## OPTIONS

**-h, --help**
: Print help information and exit

**--help-all**
: Expand all help

**--no-clean-objects**
: Do not clean object files before removing application

**REF** _TEXT_... _REQUIRED_
: Complete package reference to remove; multiple references may be specified. The format is `channel:id/version/arch`

## EXAMPLES

Remove a build result:

```bash
ll-builder remove main:org.deepin.demo/0.0.0.1/x86_64
```

## SEE ALSO

**[ll-builder(1)](./ll-builder.md)**, **[ll-builder-list(1)](list.md)**

## HISTORY

Developed in 2023 by UnionTech Software Technology Co., Ltd.
