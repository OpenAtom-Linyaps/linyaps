% ll-pica 1

## NAME

ll\-pica - Tool for converting deb packages to Linyaps packages

## SYNOPSIS

**ll-pica** _subcommand_

## DESCRIPTION

The ll-pica command provides the ability to convert deb packages to Linyaps packages, generating the linglong.yaml file needed to build Linyaps applications, and relies on ll-builder to implement application building and export. Currently, it only supports converting software packages that comply with application store packaging specifications.

Note: The conversion tool is only an auxiliary tool and cannot guarantee that the converted application will definitely run. The installation path of the software's dependent libraries or other configuration paths may not be consistent with the internal paths of Linyaps, resulting in the inability to run. You need to use the `ll-builder run --exec bash` command to enter the container for debugging.

The following situations are likely to fail to run successfully:

- Android, input method, and security software cannot be converted
- Software packages use preinst, postinst, prerm, postrm scripts
- Need to read configuration files with fixed paths
- Need to obtain root privileges

## COMMANDS

| Command | Man Page                                 | Description                                             |
| ------- | ---------------------------------------- | ------------------------------------------------------- |
| adep    | [ll-pica-adep(1)](ll-pica-adep.md)       | Add package dependencies to linglong.yaml file          |
| convert | [ll-pica-convert(1)](ll-pica-convert.md) | Convert deb packages to Linyaps packages                |
| init    | [ll-pica-init(1)](ll-pica-init.md)       | Initialize conversion package configuration information |

## SEE ALSO

**[ll-builder(1)](../ll-builder/ll-builder.md)**, **[ll-pica-adep(1)](ll-pica-adep.md)**, **[ll-pica-convert(1)](ll-pica-convert.md)**, **[ll-pica-init(1)](ll-pica-init.md)**

## HISTORY

Developed in 2024 by UnionTech Software Technology Co., Ltd.
