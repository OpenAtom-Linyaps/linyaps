% ll-builder-build 1

## NAME

ll-builder-build - Build Linyaps project

## SYNOPSIS

**ll-builder build** [*options*] [*command*...]

## DESCRIPTION

The `ll-builder build` command is used to build Linyaps applications. After the build is complete, the build content will be automatically committed to the local `ostree` cache.

The lock does not record the build architecture. Dependency references lock the channel, package ID, and exact version while preserving any explicit architecture in the original declaration. An unspecified architecture is recorded as `unknown` and uses the current build environment architecture, independently of the output target specified by `package.architecture`. A Base derived from Runtime follows the same rule using the Runtime's Base declaration. Dependencies without an explicit architecture can follow the build environment across architectures using the same lock. Those architectures must support the same locked sources, dependency versions, and module sets. A missing locked version or module for the selected architecture causes an error; another version is never selected automatically.

Normal builds reuse entries in `linglong.lock` that match the current source declarations. Matching compares `kind` and `url` for all sources. For `file`, `archive`, and `dsc`, it also compares the normalized `digest`. For `git`, it also compares `commit` and `submodules`; omitted `submodules` defaults to enabled. The `commit` and `submodules` fields do not affect matching for non-Git sources. The `name` and `version` fields do not affect matching; output names come from the current project configuration.

New or changed sources are downloaded again, even when content with the same digest is cached. A declared `digest` must match before a new lock entry is generated. Offline builds require matching source lock entries and valid cached content. With `--locked`, mismatched source declarations cause an error.

For Git sources, `commit` accepts a branch name, tag, or commit SHA. The lock stores the resolved commit as `resolved.hash: git-commit:<SHA>`. When the YAML declaration stays unchanged, builds use that locked SHA even if the remote branch or tag moves; run `ll-builder update-lockfile` to resolve it again. Changing the branch name or tag in YAML changes the source declaration: normal builds fetch it again, while `--locked` builds report an error.

File changes in the build container persist between builds. The build environment resets automatically when the exact base/runtime references or module lists change; `ll-builder clean` also removes it. Build scripts must create the temporary data they need and must not rely on files left by a previous build. Each build runs the APT installation process for the current `buildext.apt.build_depends` list; changes to that list do not reset the environment.

## OPTIONS

**-h, --help**
: Print help information and exit

**--help-all**
: Expand all help

**-f, --file** _FILE_
: Specify a project configuration file under the current working directory. If omitted, `linglong.<current-architecture>.yaml` is preferred when it exists; otherwise, `linglong.yaml` is used

**--offline**
: Use only local source caches and dependencies, without fetching either remotely; sources are still prepared and verified

**--locked**
: Require a project lock matching the declarations; cannot be used with --skip-fetch-source

**--skip-fetch-source**
: Skip obtaining sources and use the existing `linglong/sources/` as-is; useful for locally modified sources. Project lock and dependency handling still follow the normal build rules

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

Or use `--file` to specify a configuration file under the current project directory:

```bash
ll-builder build --file linglong.custom.yaml
```

Enter container to execute commands instead of building application:

```bash
ll-builder build -- bash
```

## SEE ALSO

**[ll-builder(1)](./ll-builder.md)**, **[ll-builder-export(1)](export.md)**

## HISTORY

Developed in 2023 by UnionTech Software Technology Co., Ltd.
