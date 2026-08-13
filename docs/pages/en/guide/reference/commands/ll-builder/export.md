% ll-builder-export 1

## NAME

ll-builder-export - Export Linyaps layer or UAB file

## SYNOPSIS

**ll-builder export** [*options*]

## DESCRIPTION

`ll-builder export` exports packages from the local build cache. By default it creates a distribution-mode UAB for installation and distribution. With `--uabx`, it creates a directly executable UABX containing only the application itself. The deprecated linglong layer format is also supported.

## OPTIONS

**-h, --help**
: Print help information and exit

**--help-all**
: Expand all help

**-f, --file** _FILE_
: Specify a project configuration file under the current working directory. If omitted, `linglong.<current-architecture>.yaml` is preferred when it exists; otherwise, `linglong.yaml` is used

**-o, --output** _file_
: Specify the output path for the UAB file; mutually exclusive with `--layer`

**-z, --compressor** _x_
: Specify the compression algorithm. Supports `lz4` (UAB default), `lzma` (layer default), `zstd`

**--icon** _file_
: Embed an icon in an exported UABX; requires `--uabx`

**--uabx**
: Export an exec-mode UABX containing only the selected application modules and a loader, without Base, Runtime, `uab-loader`, or `ll-box`

**--loader** _file_
: Use a custom UABX loader; requires `--uabx`. When omitted, `entry.sh` is generated from the package `command` in the application root and the default loader forwards to it

**--layer**
: **(Deprecated)** Export in layer format instead of UAB; mutually exclusive with `--icon`, `--loader`, `--output`, `--ref`, and `--modules`

**--no-develop**
: Do not export the `develop` module with layer files; must be used with `--layer`

**--ref** _ref_
: Specify the package reference to export. When omitted, resolve it from the current project; mutually exclusive with `--layer`

**--modules** _modules_
: Specify modules to export to UAB, separated by commas; mutually exclusive with `--layer`

## EXAMPLES

### Export a Distribution-mode UAB (Recommended)

A distribution-mode UAB carries the selected modules of a target ref. The package manager resolves its dependencies during installation.

```bash
# Basic export
ll-builder export

# Export a specified ref
ll-builder export --ref main:org.example.demo/1.0.0.0/x86_64

# Export UAB file using zstd compression
ll-builder export -z zstd -o my-app-zstd.uab

```

### Export an Exec-mode UABX

```bash
# Export the current project and generate the default loader
ll-builder export --uabx

# Export a specified ref and embed an icon
ll-builder export --uabx --ref main:org.example.demo/1.0.0.0/x86_64 \
  --icon assets/app.png

# Use a custom loader
ll-builder export --uabx --loader /path/to/custom/loader -o my-app.uabx
```

### Export Layer File

```bash
# Export layer format without develop module
ll-builder export --layer --no-develop

# Export layer format
ll-builder export --layer
```

## Advanced Notes

UAB and UABX use an ELF header with an EROFS bundle, but have different purposes and contents.

### Distribution Mode

The default distribution mode contains the selected modules of one ref and no loader. Install it with `ll-cli install <UABfile>` so the package manager can resolve dependencies.

### Exec Mode

An exec-mode UABX contains only the application modules and a loader. Without `--loader`, `entry.sh` is generated from the package `command` in `LINGLONG_UAB_APPROOT`, and the root loader executes that entry point. When `--loader` is specified, the custom loader is used directly. UABX is intended for direct execution and is not guaranteed to support `ll-cli install`.

## SEE ALSO

**[ll-builder(1)](./ll-builder.md)**

## HISTORY

Developed in 2023 by UnionTech Software Technology Co., Ltd.
