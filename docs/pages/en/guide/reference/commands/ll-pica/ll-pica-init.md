% ll-pica-init 1

## NAME

ll\-pica\-init - Initialize conversion package configuration information

## SYNOPSIS

**ll-pica init** [*flags*]

## DESCRIPTION

The ll-pica init command is used to initialize conversion package configuration information. This command reads the ~/.pica/config.json file to generate the package.yaml configuration file, which contains basic information for ll-pica to convert deb packages, including the base and runtime versions for building, as well as the deb package information to be converted.

## OPTIONS

**-a, --arch** _string_
: Runtime architecture

**-c, --config** _string_
: Configuration file

**--dv** _string_
: Distribution version

**--pi** _string_
: Package ID

**--pn** _string_
: Package name

**-s, --source** _string_
: Runtime source

**-t, --type** _string_
: Acquisition type

**-v, --version** _string_
: Runtime version

**-w, --workdir** _string_
: Working directory

**-V, --verbose**
: Verbose output

## EXAMPLES

Get package information from repository:

```bash
ll-pica init -w w --pi com.baidu.baidunetdisk --pn com.baidu.baidunetdisk -t repo
```

Specify UOS 20 version as BASE and RUNTIME:

```bash
ll-pica init --rv "20.0.0" --bv "20.0.0" -s "https://professional-packages.chinauos.com/desktop-professional" --dv "eagle/1070"
```

Use on arm64 architecture:

```bash
ll-pica init -a "arm64"
```

## NOTES

After the package.yaml file is generated, users can modify it as needed. For detailed fields, refer to: [Conversion Configuration File Introduction](./ll-pica-manifests.md).

## SEE ALSO

**[ll-pica(1)](ll-pica.md)**, **[ll-builder(1)](ll-pica-convert.md)**

## HISTORY

Developed in 2024 by UnionTech Software Technology Co., Ltd.
