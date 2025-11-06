% ll-builder-push 1

## NAME

ll-builder-push - Push Linyaps applications to remote repository

## SYNOPSIS

**ll-builder push** [*options*]

## DESCRIPTION

The `ll-builder push` command is used to push Linyaps packages to the Linyaps remote repository. The command automatically pushes all modules or specified modules from the project to the remote repository based on the project configuration.

## OPTIONS

**-h, --help**
: Print help information and exit

**--help-all**
: Expand all help

**-f, --file** _file_ [./linglong.yaml]
: Path to the linglong.yaml file

**--repo-url** _url_
: Remote repository URL

**--repo-name** _name_
: Remote repository name

**--module** _module_
: Push a single module

## Module Push Description

When the `--module` parameter is not specified, the command automatically pushes all modules in the project, including:

- `binary` - Binary module
- `develop` - Development module
- Other custom modules defined in the project

When the `--module` parameter is specified, only the specified single module is pushed.

## EXAMPLES

Push all modules in the project to the remote repository:

```bash
ll-builder push
```

Push a specified module to the remote repository:

```bash
ll-builder push --module binary
```

Push modules to a specified remote repository:

```bash
ll-builder push --repo-url https://repo.example.com --repo-name myrepo
```

## SEE ALSO

**[ll-builder(1)](./ll-builder.md)**, **[ll-builder-export(1)](export.md)**

## HISTORY

Developed in 2023 by UnionTech Software Technology Co., Ltd.
