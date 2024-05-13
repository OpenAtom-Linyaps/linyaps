# ll-pica 简介

本工具目前提供 deb 包转换为玲珑包的能力。目前只支持转换符合应用商店打包规范的软件包，wine、安卓、输入法、安全类软件无法转换。

查看 `ll-pica` 帮助信息：

```bash
ll-pica --help
```

`ll-pica` 命令的帮助信息如下：

```bash
Convert the deb to uab. For example:
Simple:
 ll-pica init -c package -w work-dir
 ll-pica convert -c package.yaml -w work-dir
 ll-pica help

Usage:
  ll-pica [command]

Available Commands:
  adep        Add dependency packages to linglong.yaml
  convert     Convert deb to uab
  help        Help about any command
  init        init config template

Flags:
  -h, --help      help for ll-pica
  -V, --verbose   verbose output
  -v, --version   version for ll-pica

Use "ll-pica [command] --help" for more information about a command.
```
