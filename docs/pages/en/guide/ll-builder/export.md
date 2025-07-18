<!--
SPDX-FileCopyrightText: 2023-2024 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# Export

The `ll-builder export` command is used to export applications from the local build cache into a UAB (Universal Application Bundle) file. This is the recommended format. It can also export to the deprecated linglong layer file format.

## Basic Usage

```bash
ll-builder export [OPTIONS]
```

To view all available options and their detailed descriptions, run:

```bash
ll-builder export --help
```

## Main Options

- `-f, --file FILE`: Specify the path to the `linglong.yaml` configuration file (default: `./linglong.yaml`). The directory containing `linglong.yaml` serves as the project's working directory.
- `-o, --output FILE`: Specify the output file path. For UAB, this is typically the full path or filename of the `.uab` file. For layers, this is the prefix for the output filenames.
- `-z, --compressor X`: Specify the compression algorithm. Supports `lz4` (UAB default), `lzma` (layer default), `zstd`.
- `--icon FILE`: Specify an icon for the exported UAB file (UAB mode only, mutually exclusive with `--layer`).
- `--loader FILE`: Specify a custom loader for the exported UAB file (UAB mode only, mutually exclusive with `--layer`).
- `--layer`: **(Deprecated)** Export as layer file format instead of UAB (mutually exclusive with `--icon`, `--loader`).
- `--no-develop`: When exporting layer files, do not include the `develop` module.

## Exporting UAB Files (Recommended)

A UAB (Universal Application Bundle) file is a self-contained, offline-distributable file format that includes the content required to run the application. This is the recommended export format.

After execution, the `ll-builder export` command generates a `.uab` file in the project's working directory (or the path specified with `-o`), for example, `<app_id>_<version>_<arch>.uab`.

You can customize the UAB export using other options:

```bash
# Export UAB file specifying icon and output path
ll-builder export --icon assets/app.png -o dist/my-app-installer.uab

# Export UAB file using zstd compression
ll-builder export -z zstd -o my-app-zstd.uab

# Export UAB file specifying a custom loader
ll-builder export --loader /path/to/custom/loader -o my-app-custom-loader.uab
```

## Exporting Layer Files (Deprecated)

**Note: Exporting layer file format is deprecated. Using the UAB format is recommended.**

Export layer files using the command `ll-builder export --layer`. Other examples:

```bash
# Export layer format without the develop module
ll-builder export --layer --no-develop

# Export layer format and specify the output file prefix
ll-builder export --layer -o my-app
# (Generates my-app_binary.layer and my-app_develop.layer)
```

Exporting layer files generates the following files:

- `*_binary.layer`: Contains the basic files required for the application to run.
- `*_develop.layer`: (Optional) Contains files for development and debugging. This file is not generated if the `--no-develop` option is used.

## Advanced Notes

A UAB is a statically linked ELF executable intended to run on any Linux distribution. By default, exporting a UAB file uses the "bundle mode". Using the `--loader` parameter exports a UAB file in "custom loader mode".

### Bundle Mode

Bundle mode is primarily for distribution and supports self-running capabilities as much as possible. Users typically install offline-distributed applications via `ll-cli install <UABfile>`, which automatically completes the required runtime environment for the application. Once installed, the UAB application is used the same way as an application installed directly from a repository.

During export in bundle mode, the builder attempts to automatically resolve application dependencies and export necessary content (best effort). If you grant executable permissions to a bundle mode UAB file and run it, the application runs directly without being installed. In this case, the application still runs in an isolated container, but missing parts of the runtime environment will use the host system's environment directly, so the application is not guaranteed to run.

If application developers need to rely on the self-running feature, ensure the application includes the necessary runtime dependencies.

### Custom Loader Mode

A UAB file exported in custom loader mode contains only the application data and the provided custom loader. After the UAB file is unpacked and mounted, control is handed over to the custom loader, which runs outside the container environment. The environment variable `LINGLONG_UAB_APPROOT` stores the application's directory. The custom loader is responsible for initializing the application's required runtime environment, such as library search paths.

In this mode, the application needs to ensure its compatibility with the target system. The recommended practice is to build the application on the oldest supported system version and include all dependencies.
