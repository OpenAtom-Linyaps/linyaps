# ll-pica 简介

本工具目前提供 deb 包转换为玲珑包的能力，生成构建玲珑应用需要的 linglong.yaml 文件，并依赖 ll-builder 来实现应用构建和导出。目前只支持转换符合[应用商店打包规范](https://doc.chinauos.com/content/M7kCi3QB_uwzIp6HyF5J)的软件包。

:::tip

转换工具只是辅助工具，并不能保证被转换的应用一定能运行，可能软件本身依赖库的安装路径或其他配置路径与玲珑内部路径不统一，导致无法运行，需要使用 `ll-builder run --exec bash` 命令进入容器调试。

:::

以下情况大概率无法运行成功：

1. wine、安卓、输入法、安全类软件无法转换。
2. 软件包中使用了 preinst、postinst、prerm、postrm 脚本。
3. 需要读取固定路径的配置文件。
4. 需要获取 root 权限。

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
