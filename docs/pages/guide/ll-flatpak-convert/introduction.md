# ll-pica-flatpak简介

本工具由`linglong-pica`提供。它提供将flatpak包转换为玲珑包的能力，生成构建玲珑应用需要的linglong.yaml文件，并依赖 ll-builder来实现应用构建和导出。

:::tip

转换工具只是辅助工具，并不能保证被转换的应用一定能运行，可能软件本身依赖库的安装路径或其他配置路径与玲珑内部路径不统一，导致无法运行，需要使用 `ll-builder run --exec bash` 命令进入容器调试。

:::

查看 `ll-pica-flatpak` 帮助信息：

```bash
ll-pica-flatpak --help
```

`ll-pica-flatpak` 命令的帮助信息如下：

```bash
Convert the flatpak to uab. For example:
Simple:
        ll-pica-flatpak convert [flatpak name] --build

Usage:
  ll-pica [command]

Available Commands:
  convert     Convert flatpak to uab
  help        Help about any command

Flags:
  -h, --help      help for ll-pica-flatpak
```
