% ll-builder-build 1

## NAME

ll-builder-build - Build Linyaps project

## SYNOPSIS

**ll-builder build** [*options*] [*command*...]

## DESCRIPTION

The `ll-builder build` command is used to build Linyaps applications. After the build is complete, the build content will be automatically committed to the local `ostree` cache.

## OPTIONS

**-h, --help**
: Print help information and exit

**--help-all**
: Expand all help

**-f, --file** _file_ [./linglong.yaml]
: Path to the linglong.yaml file

**--offline**
: Use only local files. This implies --skip-fetch-source and --skip-pull-depend

**--skip-fetch-source**
: Skip fetching source code

**--skip-pull-depend**
: Skip pulling dependencies

**--skip-run-container**
: Skip running container

**--skip-commit-output**
: Skip committing build output

**--skip-output-check**
: Skip output checking

**--skip-strip-symbols**
: Skip stripping debug symbols

**--isolate-network**
: Build in isolated network environment

**command** -- _COMMAND_ ...
: Enter container to execute commands instead of building application

## EXAMPLES

### Basic Usage

Build application in project root directory (where linglong.yaml file is located):

```bash
ll-builder build
```

Or use the `--file` parameter to specify the configuration file path:

```bash
ll-builder build --file /path/to/linglong.yaml
```

Enter container to execute commands instead of building application:

```bash
ll-builder build -- bash
```

## SEE ALSO

**[ll-builder(1)](./ll-builder.md)**, **[ll-builder-export(1)](export.md)**

## HISTORY

Developed in 2023 by UnionTech Software Technology Co., Ltd.
