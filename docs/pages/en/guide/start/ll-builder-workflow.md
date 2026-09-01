<!--
SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# ll-builder Workflow

`ll-builder` is the Linyaps application build tool. It reads `linglong.yaml` from a project, prepares application dependencies and source code, runs the build in an isolated environment, and saves the result to the local repository for debugging, validation, and distribution.

To complete a practical build directly, read [Build Your First Linyaps Application](./build_your_first_app.md). This chapter focuses on where each command fits in the complete development workflow and how `ll-builder build` works.

## Workflow Overview

```text
Create project → Write linglong.yaml → Build → Debug and validate → Export or push
      │                 │              │             │                │
   create          metadata,         build      run --debug         export
                   dependencies,                run                 push
                   sources, script
```

| Stage | Main task | Command or reference |
| --- | --- | --- |
| Create project | Create a project directory and `linglong.yaml` template | [`ll-builder create`](../reference/commands/ll-builder/create.md) |
| Write configuration | Define application metadata, launch command, Base, Runtime, sources, and build script | [Introduction to the Build Configuration File](../building/manifests.md) |
| Build | Pull dependencies and sources, compile in an isolated environment, and commit artifacts | [`ll-builder build`](../reference/commands/ll-builder/build.md) |
| Debug | Enter a container with a debugging environment and locate problems with a debugger | [Debug a Linyaps Application](../debug/debug.md) |
| Validate execution | Run a local build directly without installing it | [`ll-builder run`](../reference/commands/ll-builder/run.md) |
| Distribute | Export a UAB or push the build to a remote repository | [`ll-builder export`](../reference/commands/ll-builder/export.md), [`ll-builder push`](../reference/commands/ll-builder/push.md) |

## Main Steps

### 1. Create a Project

Create a project with a stable reverse-domain application ID:

```bash
ll-builder create org.example.demo
cd org.example.demo
```

`ll-builder create` generates a `linglong.yaml` template. You may also create the file manually in an existing source directory.

### 2. Write the Build Configuration

At minimum, confirm these fields in `linglong.yaml`:

- `package`: application ID, name, version, kind, and description.
- `command`: launch command after the application is installed.
- `base` and optional `runtime`: base environments required for building and running.
- `sources`: source locations, versions, and fixed commits or checksums.
- `build`: commands that compile and install artifacts into `${PREFIX}`.

See [Introduction to the Build Configuration File](../building/manifests.md) for field formats and a complete example.

### 3. Build the Application

Run this command in the directory containing `linglong.yaml`:

```bash
ll-builder build
```

After the build, list the results committed to the local repository:

```bash
ll-builder list
```

To clean up a local build, use [`ll-builder remove`](../reference/commands/ll-builder/remove.md).

### 4. Debug and Validate

Run the local build directly:

```bash
ll-builder run
```

To enter the container and diagnose a dependency, path, or launch problem, use debug mode:

```bash
ll-builder run --debug -- bash
```

See [Debug a Linyaps Application](../debug/debug.md) for details about debug mode, GDB, and IDE configuration.

### 5. Export or Push

After local validation, export a UAB for offline distribution:

```bash
ll-builder export --ref main:org.example.demo/1.0.0.0/<arch>
```

Replace the example argument with the complete ref shown by `ll-builder list`. To publish to a remote repository, configure the repository and credentials, then run:

```bash
ll-builder push
```

See [Repository Management](../publishing/repositories.md) for repository configuration and publishing methods. See the [`ll-builder` Command Reference](../reference/commands/ll-builder/ll-builder.md) for every subcommand and option.

## How ll-builder Works

Only one `build`, `clean`, `run`, `export`, `push`, `generate-lockfile`, or `update-lockfile` command may run in a project working directory at a time; a competing command fails immediately. `run` holds the lock until the application exits, so the same project cannot be built or cleaned while it is running. Different configuration files selected with `-f` in the same working directory share the project lock. Commands from different working directories also exclude each other when they use the same YAML entry directory; projects whose working and YAML entry directories do not overlap may run concurrently. Keep each working and YAML entry directory's `.ll-builder.lock` out of version control and do not delete it while a command is running. The lock is released automatically on process exit, and `clean` preserves the file.

Commands that do not load a YAML file, such as `clean` or `export --ref` in a directory without a configuration file, only lock the working directory.

When a YAML file is a symbolic link, its contents are read from the target, but the resource lock is stored beside the link, with the link filename’s extension replaced by `.lock`. For example, `linglong.yaml -> /shared/app.yaml` uses `linglong.lock` beside the entry file. Automatic discovery and explicit `-f` selection follow the same rule; symbolic links in parent directories are resolved first.

You can think of `ll-builder build` as an automated assembly line: `linglong.yaml` is the assembly specification, source code is the raw material, Base and Runtime provide a consistent work environment, and the resulting artifacts are organized and stored in the local repository.

A complete build proceeds through these stages:

```text
Read configuration → Prepare sources and dependencies → Create isolated environment
    → Run build → Organize artifacts → Save and check
```

### Read the Configuration

`ll-builder` first reads `linglong.yaml` and verifies information such as the application ID, version, package kind, launch command, Base, Runtime, sources, and build script. If a required field is missing or malformed, the build stops before entering a container.

### Obtain Sources and Dependencies

`ll-builder` downloads or expands sources according to `sources` and prepares them in `linglong/sources/`. The corresponding path in the build container is `/project/linglong/sources/`. Downloaded content can be reused from cache, but a normal build prepares the source directory again each time so files modified by a previous build cannot affect the result. Obtaining sources in a separate step instead of in the build instructions makes the build reproducible, traceable, and auditable.

Source output names must be unique, and the same fetch request cannot be declared more than once, even with different `name` or `version` values. Within the same source kind, Git requests are compared by `url`, `commit`, and `submodules`; other kinds are compared by `url` and `digest`. The `commit` and `submodules` fields are ignored for non-Git sources. To use the same resource in multiple directories, declare it once and copy it in the build script.

The first build resolves sources, Base, and Runtime and writes `linglong.lock` once sources have been verified and dependencies have been acquired. Commit this file to version control; later builds verify file sources with SHA-256 and prepare Git sources at their locked commits. A normal build updates only resources whose declarations changed and keeps the locked versions of unchanged resources. Use `ll-builder generate-lockfile` to create a lock explicitly, `ll-builder update-lockfile` to refresh resources without changing their declarations, and `ll-builder build --locked` to require an existing, complete lock that matches the declarations.

The lock has a `dependencies` list. Each Base and Runtime entry records its original declaration, selected locked reference, and available module names. Modules may include `binary`, `develop`, and other names. Normal builds acquire unchanged dependencies at their locked references and resolve only dependencies whose declarations changed. `--locked` builds always use the lock without rewriting it. A file source whose content differs from the locked hash fails verification until `update-lockfile` is run explicitly. Git sources are prepared at their locked commits, so changes to cached working trees do not enter the build sources. If Git cache verification fails, online builds fetch the locked sources again and offline builds fail. Dependency content is checked by the repository's other mechanisms. Updates replace the old file once all resources are prepared. The `buildext.apt.build_depends` overlay is reset when the actual full reference (including architecture) or module list changes. APT runs on that overlay for every build.

Version 1 of the lock includes sources and `dependencies`; each dependency uses `type: base` or `type: runtime`. Dependency references lock the channel, package ID, and exact version while preserving any explicit architecture in the original declaration. An unspecified architecture is recorded as `unknown`, such as `main:org.example.Base/1.0.0.0/unknown`, and uses the current build environment architecture, independently of the output target specified by `package.architecture`. A Base derived from Runtime follows the same rule using the Runtime's Base declaration. Dependencies without an explicit architecture can reuse the same lock across build architectures without an update solely for an architecture change. All architectures must use the same locked sources, dependency versions, and module sets. Missing locked dependencies for the selected architecture cause an error rather than an automatic upgrade or fallback.

`--skip-fetch-source` only skips obtaining sources and uses the existing `linglong/sources/` as-is, which supports locally modified sources. Reading the project lock and resolving and locking Base and Runtime still follow the normal build rules. It cannot be combined with `--locked`; use `--locked --offline` to verify cached sources against the lock using only local caches.

It then prepares `base` and `runtime` for the build environment.

### Create the Isolated Build Environment

Before compiling, `ll-builder`:

1. Cleans temporary directories from the previous build, such as `linglong/output/`. The `linglong` directory is an internal `ll-builder` work directory and must not be used to store other files.
2. Combines the Base, Runtime, project directory, sources, and output directory into the build environment, and creates temporary writable layers for the Base and Runtime so the original dependencies are not modified.
3. Installs build-time dependencies such as `buildext.apt.build_depends`.

The environment is a container isolated from the host, but it can access the current project and prepared sources. Two commonly used variables are available inside it:

```text
PREFIX=<installation prefix of the current package>
TRIPLET=<GNU triplet for the current CPU architecture>
```

`ll-builder` then executes the `build` script from `linglong.yaml` in the container. Executables, libraries, and resources produced by the script should be installed into `${PREFIX}`. Content written to `${PREFIX}` enters the final package; content written directly to `/usr` exists only in the temporary build environment and is not included as an application artifact.

### Organize and Save the Build Result

After the build script succeeds, `ll-builder` collects artifacts from `${PREFIX}`, generates the application configuration, and splits content according to `modules`:

- `binary` normally contains files required to run the application.
- `develop` normally contains headers, static libraries, and debugging information.
- Custom modules contain optional content defined by the project.

Desktop files, icons, service configuration, and other content requiring desktop integration are also organized separately at this stage. Each module is then saved to the local OSTree repository.

Finally, an App is checked in the combination of its declared Base and Runtime to detect problems such as missing dynamic libraries or incorrect file locations early. A successful application can be run directly with `ll-builder run`, or exported or pushed.

An application package contains the content it must deliver. At runtime, that content is recombined with components such as Base and Runtime into a filesystem inside a rootless container. See [Basic Concepts](../reference/basic-concepts.md) for the relationships among package kinds, containers, OSTree, and the tools.

### How build Options Affect the Workflow

The following options alter build inputs or how stages are executed. `-h, --help` and `--help-all` only display help and exit.

| Option | Affected stage | Effect |
| --- | --- | --- |
| `-f, --file <file>` | Read configuration | Use the specified file instead of an automatically discovered `linglong.<arch>.yaml` or `linglong.yaml`. The file must be in the current project directory or one of its subdirectories |
| `--offline` | Obtain sources and dependencies | Use only source caches verified by the lock and do not pull Base or Runtime remotely. All required content must already exist locally. `offline: true` in the ll-builder configuration file (default: `~/.config/linglong/builder/config.yaml`) has the same effect |
| `--locked` | Obtain sources and dependencies | Require an existing project lock matching the source, Base, and Runtime declarations; never update the lock during the build |
| `--skip-fetch-source` | Obtain sources | Skip obtaining sources and use the existing `linglong/sources/` as-is; project lock and dependency handling still follow the normal build rules; cannot be combined with `--locked` |
| `--skip-pull-depend` | Obtain dependencies | Do not pull new content from remotes, but still resolve and merge locally available Base and Runtime modules. The build fails if a dependency is missing locally |
| `--skip-run-container` | Run build | Prepare the configuration, sources, and dependencies, but do not clean output, create the build container, or run the build script. The command then exits without organizing or checking output |
| `--skip-commit-output` | Save artifacts | Run the container build normally, but do not generate modules, desktop-integration content, or application configuration, and do not save to the local repository. The runtime check is also skipped |
| `--skip-output-check` | Check artifacts | Run checks and collect information required to export a UAB, but do not fail the build if a check fails |
| `--skip-strip-symbols` | Run build | Disable automatic symbol separation and stripping by `ll-builder`. Whether the result contains debugging information still depends on the project's own compiler options |
| `--isolate-network` | Run build | Disconnect only the main build container that executes the `build` script. Source fetching and Base and Runtime pulls happen earlier and are unaffected |
| `COMMAND...` | Run build | Use the specified command instead of the `build` script in `linglong.yaml`. This is useful for diagnostics or manual building in the same environment. After a successful exit, artifacts are still organized and checked; combine it with `--skip-commit-output` for diagnostics only |

Both `--skip-run-container` and `--skip-commit-output` end the remaining workflow early. They are useful for diagnosing stages but do not produce a new build that can be used by `ll-builder run`. See [`ll-builder build`](../reference/commands/ll-builder/build.md) for the command-line definitions.
