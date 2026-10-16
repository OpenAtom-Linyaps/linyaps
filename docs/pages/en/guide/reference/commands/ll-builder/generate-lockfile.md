% ll-builder-generate-lockfile 1

## NAME

ll-builder-generate-lockfile - Generate a build resource lock file

## SYNOPSIS

**ll-builder generate-lockfile** [*options*]

## DESCRIPTION

`ll-builder generate-lockfile` acquires the sources and Base/Runtime dependencies declared by the project, then records source hashes, Git commits, exact dependency references, and module lists. It writes a complete resource lock file after resource preparation succeeds. This command does not execute the project's build script.

The corresponding lock file must not exist. If it already exists, the command fails; use `ll-builder update-lockfile` instead. Failed resource preparation does not produce a complete lock, but acquired content may remain in the cache for later use.

The lock is stored beside the YAML entry file, with its extension replaced by `.lock`: `linglong.yaml` uses `linglong.lock`, and `config/app.yaml` uses `config/app.lock`. See [ll-builder(1)](./ll-builder.md) for file symbolic link and project locking rules.

Commit the generated lock alongside the project configuration. Subsequent builds can use `ll-builder build --locked` to build with the locked resources.

## OPTIONS

**-h, --help**
: Print help information and exit

**--help-all**
: Expand all help

**-f, --file** _FILE_
: Select a project YAML file within the current working directory, including its subdirectories. If omitted, use `linglong.<current build architecture>.yaml` when present, otherwise `linglong.yaml`

## EXAMPLES

Generate a resource lock for a project that has none, then build:

```bash
ll-builder generate-lockfile
ll-builder build --locked
```

Generate `config/app.lock` for a configuration in a subdirectory:

```bash
ll-builder generate-lockfile -f config/app.yaml
```

## SEE ALSO

**[ll-builder(1)](./ll-builder.md)**, **[ll-builder-build(1)](./build.md)**, **[ll-builder-update-lockfile(1)](./update-lockfile.md)**

## HISTORY

Developed in 2026 by UnionTech Software Technology Co., Ltd.
