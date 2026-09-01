% ll-builder-update-lockfile 1

## NAME

ll-builder-update-lockfile - Update a build resource lock file

## SYNOPSIS

**ll-builder update-lockfile** [*options*]

## DESCRIPTION

`ll-builder update-lockfile` resolves all sources and Base/Runtime dependencies from the current project YAML again, then replaces the corresponding resource lock file in a single step after resource preparation succeeds. It resolves Git branches, tags, and dependency version requirements again even when their declarations have not changed. Any YAML `digest` remains a mandatory verification condition. This command does not execute the project's build script.

The corresponding lock file must already exist. If it does not, the command fails; use `ll-builder generate-lockfile` first. A failed update preserves the old lock, but acquired content may remain in the cache for later use.

The lock is stored beside the YAML entry file, with its extension replaced by `.lock`. See [ll-builder(1)](./ll-builder.md) for file symbolic link and project locking rules.

The updated `build_architecture` records the current build environment's architecture. Use this command to lock resources again after moving a project to a different build architecture. This field is independent of the output target architecture specified by `package.architecture`.

Ordinary builds reuse locked resources whose declarations have not changed. Use this command to refresh resources explicitly, then review and commit the lock changes before rebuilding.

## OPTIONS

**-h, --help**
: Print help information and exit

**--help-all**
: Expand all help

**-f, --file** _FILE_
: Select a project YAML file within the current working directory, including its subdirectories. If omitted, use `linglong.<current build architecture>.yaml` when present, otherwise `linglong.yaml`

## EXAMPLES

Update an existing resource lock, review its changes, then build:

```bash
ll-builder update-lockfile
git diff -- linglong.lock
ll-builder build --locked
```

Update `config/app.lock` for a configuration in a subdirectory:

```bash
ll-builder update-lockfile -f config/app.yaml
```

## SEE ALSO

**[ll-builder(1)](./ll-builder.md)**, **[ll-builder-build(1)](./build.md)**, **[ll-builder-generate-lockfile(1)](./generate-lockfile.md)**

## HISTORY

Developed in 2026 by UnionTech Software Technology Co., Ltd.
