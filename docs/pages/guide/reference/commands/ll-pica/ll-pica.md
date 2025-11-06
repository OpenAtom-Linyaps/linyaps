% ll-pica 1

## NAME

ll\-pica - 将 deb 包转换为玲珑包的工具

## SYNOPSIS

**ll-pica** _subcommand_

## DESCRIPTION

ll-pica 命令提供将 deb 包转换为玲珑包的能力，生成构建玲珑应用需要的 linglong.yaml 文件，并依赖 ll-builder 来实现应用构建和导出。目前只支持转换符合应用商店打包规范的软件包。

注意：转换工具只是辅助工具，并不能保证被转换的应用一定能运行。可能软件本身依赖库的安装路径或其他配置路径与玲珑内部路径不统一，导致无法运行，需要使用 `ll-builder run --exec bash` 命令进入容器调试。

以下情况大概率无法运行成功：

- 安卓、输入法、安全类软件无法转换
- 软件包中使用了 preinst、postinst、prerm、postrm 脚本
- 需要读取固定路径的配置文件
- 需要获取 root 权限

## COMMANDS

| Command | Man Page                                 | Description                     |
| ------- | ---------------------------------------- | ------------------------------- |
| adep    | [ll-pica-adep(1)](ll-pica-adep.md)       | 给 linglong.yaml 文件添加包依赖 |
| convert | [ll-pica-convert(1)](ll-pica-convert.md) | 将 deb 包转换为玲珑包           |
| init    | [ll-pica-init(1)](ll-pica-init.md)       | 初始化转换包的配置信息          |

## SEE ALSO

**[ll-builder(1)](../ll-builder/ll-builder.md)**, **[ll-pica-adep(1)](ll-pica-adep.md)**, **[ll-pica-convert(1)](ll-pica-convert.md)**, **[ll-pica-init(1)](ll-pica-init.md)**

## HISTORY

2024年，由 UnionTech Software Technology Co., Ltd. 开发
