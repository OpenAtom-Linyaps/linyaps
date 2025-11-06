% ll-appimage-convert 1

## NAME

ll\-appimage\-convert - 将 AppImage 包转换为如意玲珑包格式

## SYNOPSIS

**ll-appimage-convert** _subcommand_

## DESCRIPTION

ll-appimage-convert 命令用于将 AppImage 包格式（.appimage 或 .AppImage）转换为如意玲珑包格式（.layer 或 .uab）。该工具会生成构建如意玲珑应用需要的 linglong.yaml 文件，并依赖 ll-builder 来实现应用构建和导出。

注意：转换工具只是辅助工具，并不能保证被转换的应用一定能运行。可能软件本身依赖库的安装路径或其他配置路径与如意玲珑内部路径不统一，导致无法运行，需要使用 `ll-builder run --exec bash` 命令进入容器调试。

## COMMANDS

| Command | Man Page                                                         | Description                        |
| ------- | ---------------------------------------------------------------- | ---------------------------------- |
| convert | [ll-appimage-convert-convert(1)](ll-appimage-convert-convert.md) | 将 AppImage 包转换为如意玲珑包格式 |

## SEE ALSO

**[ll-appimage-convert-convert](ll-appimage-convert-convert.md)**

## HISTORY

2024年，由 UnionTech Software Technology Co., Ltd. 开发
