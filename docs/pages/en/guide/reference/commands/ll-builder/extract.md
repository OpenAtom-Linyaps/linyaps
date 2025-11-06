% ll-builder-extract 1

## NAME

ll-builder-extract - Extract Linyaps layer file to directory

## SYNOPSIS

**ll-builder extract** [*options*] _file_ _directory_

## DESCRIPTION

The `ll-builder extract` command is used to extract Linyaps layer files to a specified directory.

## OPTIONS

**-h, --help**
: Print help information and exit

**--help-all**
: Expand all help

**file** (required)
: Path to the layer file to be extracted

**directory** (required)
: Target directory for extraction

## EXAMPLES

Extract layer file to specified directory:

```bash
ll-builder extract org.deepin.demo_binary.layer /tmp/extracted
```

## SEE ALSO

**[ll-builder(1)](./ll-builder.md)**, **[ll-builder-export(1)](export.md)**

## HISTORY

Developed in 2023 by UnionTech Software Technology Co., Ltd.
