% ll-builder-import 1

## NAME

ll-builder-import - Import Linyaps layer file to build repository

## SYNOPSIS

**ll-builder import** [*options*] _file_

## DESCRIPTION

The `ll-builder import` command is used to import Linyaps layer files to the build repository.

## OPTIONS

**-h, --help**
: Print help information and exit

**--help-all**
: Expand all help

**file** (required)
: Path to the layer file to be imported

## EXAMPLES

Import layer file to build repository:

```bash
ll-builder import org.deepin.demo_binary.layer
```

## SEE ALSO

**[ll-builder(1)](./ll-builder.md)**, **[ll-builder-export(1)](export.md)**

## HISTORY

Developed in 2023 by UnionTech Software Technology Co., Ltd.
