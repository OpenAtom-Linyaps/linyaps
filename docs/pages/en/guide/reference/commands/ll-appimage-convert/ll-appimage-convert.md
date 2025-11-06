% ll-appimage-convert 1

## NAME

ll\-appimage\-convert - Convert AppImage packages to Linyaps package format

## SYNOPSIS

**ll-appimage-convert** _subcommand_

## DESCRIPTION

The ll-appimage-convert command is used to convert AppImage package formats (.appimage or .AppImage) to Linyaps package formats (.layer or .uab). This tool generates the linglong.yaml file needed to build Linyaps applications and relies on ll-builder to implement application building and export.

Note: The conversion tool is only an auxiliary tool and cannot guarantee that the converted application will run. The software itself may have dependency library installation paths or other configuration paths that are inconsistent with Linyaps internal paths, causing it to fail to run. You need to use the `ll-builder run --exec bash` command to enter the container for debugging.

## COMMANDS

| Command | Man Page                                                         | Description                                         |
| ------- | ---------------------------------------------------------------- | --------------------------------------------------- |
| convert | [ll-appimage-convert-convert(1)](ll-appimage-convert-convert.md) | Convert AppImage packages to Linyaps package format |

## SEE ALSO

**[ll-appimage-convert-convert](ll-appimage-convert-convert.md)**

## HISTORY

Developed in 2024 by UnionTech Software Technology Co., Ltd.
