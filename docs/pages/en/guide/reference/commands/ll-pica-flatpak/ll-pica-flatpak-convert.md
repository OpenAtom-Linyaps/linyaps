% ll-pica-flatpak-convert 1

## NAME

ll\-pica\-flatpak\-convert - Convert Flatpak applications to Linyaps applications

## SYNOPSIS

**ll-pica-flatpak convert** [*flags*]

## DESCRIPTION

The ll-pica-flatpak convert command is used to convert Flatpak applications to Linyaps applications. This command uses the ostree command to pull Flatpak application data and generates the corresponding Linyaps base environment based on the runtime defined in the metadata.

By default, the converted application generates a uab file. To convert to a layer file, you need to add the --layer parameter.

## OPTIONS

**--build**
: Build Linyaps package

**--layer**
: Export layer file

**--version** _string_
: Specify the version of the generated Linyaps application

**--base** _string_
: Specify the base environment of the Linyaps application

**--base-version** _string_
: Specify the base version of the Linyaps application

## EXAMPLES

Convert Flatpak application org.videolan.VLC:

```bash
ll-pica-flatpak convert org.videolan.VLC --build
```

Convert application and generate layer file:

```bash
ll-pica-flatpak convert org.videolan.VLC --build --layer
```

Specify the version of the generated Linyaps application:

```bash
ll-pica-flatpak convert org.videolan.VLC --version "1.0.0.0" --build --layer
```

Specify the base environment and version of the Linyaps application:

```bash
ll-pica-flatpak convert org.videolan.VLC --base "org.deepin.base.flatpak.kde" --base-version "6.7.0.2" --build --layer
```

## SEE ALSO

**[ll-pica-flatpak(1)](ll-pica-flatpak.md)**, **[ll-builder(1)](../ll-builder/ll-builder.md)**

## HISTORY

Developed in 2024 by UnionTech Software Technology Co., Ltd.
