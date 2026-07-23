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

It then prepares `base` and `runtime` for the build environment.

### Create the Isolated Build Environment

Before compiling, `ll-builder`:

1. Cleans temporary directories from the previous build, such as `linglong/output/`. The `linglong` directory is an internal `ll-builder` work directory and must not be used to store other files.
2. Combines the Base, Runtime, project directory, sources, and output directory into the build environment, and creates temporary writable layers for the Base and Runtime so the original dependencies are not modified.
3. Installs build-time dependencies such as `buildext.apt.buildDepends`.

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
| `--offline` | Obtain sources and dependencies | Do not fetch sources or pull Base and Runtime from remote repositories. All required sources and dependencies must already exist locally. `offline: true` in `linglong.yaml` has the same effect |
| `--skip-fetch-source` | Obtain sources | Preserve and use the existing `linglong/sources/` without cleaning or fetching `sources` |
| `--skip-pull-depend` | Obtain dependencies | Do not pull new content from remotes, but still resolve and merge locally available Base and Runtime modules. The build fails if a dependency is missing locally |
| `--skip-run-container` | Run build | Prepare the configuration, sources, and dependencies, but do not clean output, create the build container, or run the build script. The command then exits without organizing or checking output |
| `--skip-commit-output` | Save artifacts | Run the container build normally, but do not generate modules, desktop-integration content, or application configuration, and do not save to the local repository. The runtime check is also skipped |
| `--skip-output-check` | Check artifacts | Run checks and collect information required to export a UAB, but do not fail the build if a check fails |
| `--skip-strip-symbols` | Run build | Disable automatic symbol separation and stripping by `ll-builder`. Whether the result contains debugging information still depends on the project's own compiler options |
| `--isolate-network` | Run build | Disconnect only the main build container that executes the `build` script. Source fetching and Base and Runtime pulls happen earlier and are unaffected |
| `COMMAND...` | Run build | Use the specified command instead of the `build` script in `linglong.yaml`. This is useful for diagnostics or manual building in the same environment. After a successful exit, artifacts are still organized and checked; combine it with `--skip-commit-output` for diagnostics only |

Both `--skip-run-container` and `--skip-commit-output` end the remaining workflow early. They are useful for diagnosing stages but do not produce a new build that can be used by `ll-builder run`. See [`ll-builder build`](../reference/commands/ll-builder/build.md) for the command-line definitions.
