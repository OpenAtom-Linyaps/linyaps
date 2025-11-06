% ll-builder-export 1

## NAME

ll-builder-export - Export Linyaps layer or UAB file

## SYNOPSIS

**ll-builder export** [*options*]

## DESCRIPTION

The `ll-builder export` command is used to export applications from the local build cache as UAB (Universal Application Bundle) files. This is the recommended format. It can also export to the deprecated linglong layer file format.

## OPTIONS

**-h, --help**
: Print help information and exit

**--help-all**
: Expand all help

**-f, --file** _file_ [./linglong.yaml]
: Specify the path to the `linglong.yaml` configuration file. The directory where the `linglong.yaml` file is located is the project's working directory

**-o, --output** _file_
: Specify the path for the output file. For UAB, this is usually the full path or filename of the `.uab` file. For layer, this is the prefix for the output filename

**-z, --compressor** _x_
: Specify the compression algorithm. Supports `lz4` (UAB default), `lzma` (layer default), `zstd`

**--icon** _file_
: Specify an icon for the exported UAB file (UAB mode only, mutually exclusive with `--layer`)

**--loader** _file_
: Specify a custom loader for the exported UAB file (UAB mode only, mutually exclusive with `--layer`)

**--layer**
: **(Deprecated)** Export as layer file format instead of UAB (mutually exclusive with `--icon`, `--loader`)

**--no-develop**
: Do not export the `develop` module when exporting layer files

**--ref** _ref_
: Specify package reference

**--modules** _modules_
: Specify modules to export

## EXAMPLES

### Export UAB File (Recommended)

UAB (Universal Application Bundle) file is a self-contained, offline-distributable file format that includes all content required for application runtime. This is the recommended export format.

```bash
# Basic export
ll-builder export

# Export UAB file and specify icon and output path
ll-builder export --icon assets/app.png -o dist/my-app-installer.uab

# Export UAB file using zstd compression
ll-builder export -z zstd -o my-app-zstd.uab

# Export UAB file and specify custom loader
ll-builder export --loader /path/to/custom/loader -o my-app-custom-loader.uab
```

### Export Layer File

```bash
# Export layer format without develop module
ll-builder export --layer --no-develop

# Export layer format and specify output file prefix
ll-builder export --layer -o my-app
# (Will generate my-app_binary.layer and my-app_develop.layer)
```

## Advanced Notes

UAB is a statically linked ELF executable file whose goal is to run on any Linux distribution. By default, exported UAB files belong to bundle mode, while UAB files exported using the `--loader` parameter belong to custom loader mode.

### Bundle Mode

Bundle mode is mainly used for distribution and supports self-running functionality as much as possible. Users typically install offline-distributed applications through `ll-cli install <UABfile>`, and the application runtime environment is automatically completed.

### Custom Loader Mode

UAB files exported in custom loader mode contain only application data and the passed custom loader. After the UAB file is extracted and mounted, control is handed over to the custom loader, which is no longer in the container environment.

## SEE ALSO

**[ll-builder(1)](./ll-builder.md)**

## HISTORY

Developed in 2023 by UnionTech Software Technology Co., Ltd.
