% ll-builder 1

## NAME

ll-builder - Linyaps application build tool

## SYNOPSIS

**ll-builder** [*options*] _subcommand_

## DESCRIPTION

ll-builder is a tool provided for application developers to build Linyaps applications. It supports building applications in isolated containers and includes a complete push and publish workflow.

Main features include:

- Building applications in isolated containers
- Complete application build, test, export, and publish workflow

Only one `build`, `clean`, `run`, `export`, `push`, `generate-lockfile`, or `update-lockfile` command may operate in a project working directory at a time. A competing command fails immediately without waiting. For example, while an application is running through `ll-builder run`, the same project cannot be built or cleaned. Selecting different configuration files with `-f` in the same working directory shares the project lock. Commands from different working directories also exclude each other when they use the same YAML entry directory. Projects whose working and YAML entry directories do not overlap may be used concurrently.

The project lock uses `.ll-builder.lock` in both the working directory and the YAML entry directory. When these are the same directory, the lock is acquired once. It is released automatically when the command ends or its process exits. Keep this file out of version control. `clean` preserves it, and its presence does not mean the project is busy. Do not delete it while a command is running. Commit the resource lock file `linglong.lock` to version control.

Commands that do not load a YAML file, such as `clean` or `export --ref` in a directory without a configuration file, only lock the working directory.

When a YAML file is a symbolic link, its contents are read from the target, but the resource lock is stored beside the link, with the link filename’s extension replaced by `.lock`. For example, `linglong.yaml -> /shared/app.yaml` uses `linglong.lock` beside the entry file. Automatic discovery and explicit `-f` selection follow the same rule; symbolic links in parent directories are resolved first.

## OPTIONS

**-h, --help**
: Print help information and exit

**--help-all**
: Expand all help

**--version**
: Display version information

## COMMANDS

| Command | Man Page                              | Description                                   |
| ------- | ------------------------------------- | --------------------------------------------- |
| create  | [ll-builder-create(1)](./create.md)   | Create Linyaps build template                 |
| build   | [ll-builder-build(1)](./build.md)     | Build Linyaps project                         |
| generate-lockfile | [ll-builder-generate-lockfile(1)](./generate-lockfile.md) | Generate a build resource lock without compiling |
| update-lockfile | [ll-builder-update-lockfile(1)](./update-lockfile.md) | Resolve resources again and update an existing lock |
| run     | [ll-builder-run(1)](./run.md)         | Run the built application                     |
| list    | [ll-builder-list(1)](./list.md)       | List built applications                       |
| remove  | [ll-builder-remove(1)](./remove.md)   | Remove built applications                     |
| export  | [ll-builder-export(1)](./export.md)   | Export Linyaps layer or UAB file              |
| push    | [ll-builder-push(1)](./push.md)       | Push Linyaps application to remote repository |
| import  | [ll-builder-import(1)](./import.md)   | Import Linyaps layer file to build repository |
| extract | [ll-builder-extract(1)](./extract.md) | Extract Linyaps layer file to directory       |
| repo    | [ll-builder-repo(1)](./repo.md)       | Display and manage repository                 |

## SEE ALSO

**[ll-builder-generate-lockfile(1)](./generate-lockfile.md)**, **[ll-builder-update-lockfile(1)](./update-lockfile.md)**, **[ll-builder-create(1)](./create.md)**, **[ll-builder-build(1)](./build.md)**, **[ll-builder-run(1)](./run.md)**, **[ll-builder-export(1)](./export.md)**, **[ll-builder-push(1)](./push.md)**

## HISTORY

Developed in 2023 by UnionTech Software Technology Co., Ltd.
