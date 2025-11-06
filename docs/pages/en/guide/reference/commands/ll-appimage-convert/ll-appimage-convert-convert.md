% ll-appimage-convert-convert 1

## NAME

ll\-appimage\-convert\-convert - Convert AppImage packages to Linyaps package format

## SYNOPSIS

**ll-appimage-convert convert** [*flags*]

## DESCRIPTION

The ll-appimage-convert convert command generates a directory based on the specified application name, which serves as the root directory of the Linyaps project, i.e., the location of the linglong.yaml file. It supports two conversion methods:

1. Use the `--file` option to convert the specified appimage file to a Linyaps package file
2. Use the `--url` and `--hash` options to convert the specified appimage URL and hash value to a Linyaps package file
3. Use the `--layer` option to export .layer format files, otherwise it will export .uab format files by default

When the Linyaps version is greater than 1.5.7, convert exports uab packages by default. If you want to export layer files, you need to add the --layer parameter.

You can use the `--output` option to generate Linyaps project configuration files (linglong.yaml) and build scripts for Linyaps .layer (.uab) files, and then execute the script to generate the corresponding Linyaps package when the linglong.yaml configuration file is modified. If this option is not specified, the corresponding Linyaps package will be exported directly.

## OPTIONS

**-b, --build**
: Build Linyaps package

**-d, --description** _string_
: Detailed description of the package

**-f, --file** _string_
: App package file, this option is not required when --url option and --hash option are set

**--hash** _string_
: Package hash value, must be used together with --url option

**-i, --id** _string_
: Unique name of the package

**-l, --layer**
: Export layer file

**-n, --name** _string_
: Descriptive name of the package

**-u, --url** _string_
: Package URL, this option is not required when -f option is set

**-v, --version** _string_
: Version of the package

**-V, --verbose**
: Verbose output

## EXAMPLES

Convert BrainWaves appimage file to Linyaps .layer file via --url option:

```bash
ll-appimage-convert convert --url "https://github.com/makebrainwaves/BrainWaves/releases/download/v0.15.1/BrainWaves-0.15.1.AppImage" --hash "04fcfb9ccf5c0437cd3007922fdd7cd1d0a73883fd28e364b79661dbd25a4093" --name "io.github.brainwaves" --id "io.github.brainwaves" --version "0.15.1.0" --description "io.github.brainwaves" -b
```

Convert BrainWaves-0.15.1.AppImage to Linyaps .uab via --file option:

```bash
ll-appimage-convert convert -f ~/Downloads/BrainWaves-0.15.1.AppImage --name "io.github.brainwaves" --id "io.github.brainwaves" --version "0.15.1.0" --description "io.github.brainwaves" -b
```

## SEE ALSO

**[ll-appimage-convert(1)](ll-appimage-convert.md)**, **[ll-builder(1)](../ll-builder/ll-builder.md)**

## HISTORY

Developed in 2024 by UnionTech Software Technology Co., Ltd.
