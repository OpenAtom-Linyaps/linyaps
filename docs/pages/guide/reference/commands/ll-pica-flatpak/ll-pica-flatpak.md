% ll-pica-flatpak 1

## NAME

ll\-pica\-flatpak - 将 Flatpak 应用转换为玲珑应用

## SYNOPSIS

**ll-pica-flatpak** _subcommand_

## DESCRIPTION

ll-pica-flatpak 命令用于将 Flatpak 应用转换为玲珑应用。该工具使用 ostree 命令拉取 Flatpak 应用数据，根据 metadata 中定义的 runtime 对应生成玲珑的 base 环境，并生成构建玲珑应用需要的 linglong.yaml 文件。

注意：转换工具只是辅助工具，并不能保证被转换的应用一定能运行。可能软件本身依赖库的安装路径或其他配置路径与玲珑内部路径不统一，导致无法运行，需要使用 `ll-builder run --exec bash` 命令进入容器调试。

## COMMANDS

| Command | Man Page                                                 | Description                   |
| ------- | -------------------------------------------------------- | ----------------------------- |
| convert | [ll-pica-flatpak-convert(1)](ll-pica-flatpak-convert.md) | 将 Flatpak 应用转换为玲珑应用 |

## SEE ALSO

**[ll-builder(1)](../ll-builder/ll-builder.md)**, **[ll-pica-flatpak-convert(1)](ll-pica-flatpak-convert.md)**

## HISTORY

2024年，由 UnionTech Software Technology Co., Ltd. 开发
