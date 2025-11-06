% ll-pica-convert 1

## NAME

ll\-pica\-convert - Convert deb packages to Linyaps packages

## SYNOPSIS

**ll-pica convert** [*flags*]

## DESCRIPTION

The ll-pica convert command is used to generate the linglong.yaml file needed by Linyaps and convert deb packages to Linyaps packages. This command supports converting from local deb files or downloading deb packages from repositories through the apt download command.

## OPTIONS

**-b, --build**
: Build Linyaps package

**-c, --config** _string_
: Configuration file

**--exportFile** _string_
: Export uab or layer (defaults to "uab")

**--pi** _string_
: Package ID

**--pn** _string_
: Package name

**-t, --type** _string_
: Get application type (defaults to "local")

**--withDep**
: Add dependency tree

**-w, --workdir** _string_
: Working directory

**-V, --verbose**
: Verbose output

## EXAMPLES

Download deb package from repository and convert:

```bash
ll-pica init -w w --pi com.baidu.baidunetdisk --pn com.baidu.baidunetdisk -t repo
```

```bash
ll-pica convert -w w -b --exportFile
```

Convert from local deb file:

```bash
ll-pica convert -c com.baidu.baidunetdisk_4.17.7_amd64.deb -w w -b --exportFile layer
```

## SEE ALSO

**[ll-pica(1)](ll-pica.md)**, **[ll-cli(1)](ll-pica-init.md)**

## HISTORY

Developed in 2024 by UnionTech Software Technology Co., Ltd.
