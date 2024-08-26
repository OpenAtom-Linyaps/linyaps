# ll-appimage-convert 简介

本工具目前提供`appimage`包转换为玲珑包的能力，生成构建玲珑应用需要的 linglong.yaml 文件，并依赖 ll-builder 来实现应用构建和导出。


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
