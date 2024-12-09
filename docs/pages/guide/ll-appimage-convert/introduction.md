# ll-appimage-convert简介

本工具由`linglong-pica`包提供。它提供将appimage包转换为如意玲珑包的能力，生成构建如意玲珑应用需要的linglong.yaml文件，并依赖 ll-builder 来实现应用构建和导出。

:::tip

转换工具只是辅助工具，并不能保证被转换的应用一定能运行，可能软件本身依赖库的安装路径或其他配置路径与如意玲珑内部路径不统一，导致无法运行，需要使用 `ll-builder run --exec bash` 命令进入容器调试。

:::

查看 `ll-appimage-convert` 帮助信息：

```bash
ll-appimage-convert --help
```

`ll-appimage-convert` 命令的帮助信息如下：

```bash
Convert the appimage to uab. For example:
 Simple:
         ll-appimage-convert  convert -f xxx.appimage -i "io.github.demo" -n "io.github.demo" -v "1.0.0.0" -d "this is a appimage convert demo" -b
         ll-appimage-convert help

Usage:
  ll-appimage-convert [command]

Available Commands:
  convert     Convert appimage to uab
  help        Help about any command

Flags:
  -h, --help      help for ll-appimage-convert
  -V, --verbose   verbose output
  -v, --version   version for ll-appimage-convert

Use "ll-appimage-convert [command] --help" for more information about a command.
```
