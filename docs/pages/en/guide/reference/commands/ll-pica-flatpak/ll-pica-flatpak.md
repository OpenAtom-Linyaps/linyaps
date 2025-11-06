% ll-pica-flatpak 1

## NAME

ll\-pica\-flatpak - Convert Flatpak applications to Linyaps applications

## SYNOPSIS

**ll-pica-flatpak** _subcommand_

## DESCRIPTION

The ll-pica-flatpak command is used to convert Flatpak applications to Linyaps applications. This tool uses the ostree command to pull Flatpak application data, generates the corresponding Linyaps base environment based on the runtime defined in the metadata, and generates the linglong.yaml file needed to build Linyaps applications.

Note: The conversion tool is only an auxiliary tool and cannot guarantee that the converted application will definitely run. The installation path of the software's dependent libraries or other configuration paths may not be consistent with the internal paths of Linyaps, resulting in the inability to run. You need to use the `ll-builder run --exec bash` command to enter the container for debugging.

## COMMANDS

| Command | Man Page                                                 | Description                                          |
| ------- | -------------------------------------------------------- | ---------------------------------------------------- |
| convert | [ll-pica-flatpak-convert(1)](ll-pica-flatpak-convert.md) | Convert Flatpak applications to Linyaps applications |

## SEE ALSO

**[ll-builder(1)](../ll-builder/ll-builder.md)**, **[ll-pica-flatpak-convert(1)](ll-pica-flatpak-convert.md)**

## HISTORY

Developed in 2024 by UnionTech Software Technology Co., Ltd.
