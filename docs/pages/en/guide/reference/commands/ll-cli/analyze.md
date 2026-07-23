% ll-cli-analyze 1

## NAME

ll\-cli\-analyze - Analyze installed applications

## SYNOPSIS

**ll-cli analyze** _subcommand_ [*options*]

## DESCRIPTION

`ll-cli analyze` analyzes the disk usage of installed modules and application dependency relationships. A subcommand is required.

## SUBCOMMANDS

### size

Show the disk usage of installed modules and the actual repository usage.

**Usage**: `ll-cli analyze size [OPTIONS]`

**--sort** _FIELD_ [*actual*]
: Specify the sort field. Valid values are `actual`, `logical`, `exclusive`, `shared`, and `id`

**--asc**
: Sort in ascending order; the default is descending order

```bash
ll-cli analyze size
ll-cli analyze size --sort exclusive --asc
```

### depends

Show the dependency tree of installed applications. If no application is specified, show the dependency trees of all installed applications.

**Usage**: `ll-cli analyze depends [APP]`

```bash
ll-cli analyze depends
ll-cli analyze depends org.deepin.demo
```

## OPTIONS

**-h, --help**
: Print help information and exit

**--help-all**
: Show all help

## SEE ALSO

**[ll-cli(1)](./ll-cli.md)**, **[ll-cli-list(1)](./list.md)**, **[ll-cli-info(1)](./info.md)**

## HISTORY

Developed by UnionTech Software Technology Co., Ltd. in 2026
