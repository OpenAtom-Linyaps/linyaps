# 初始化配置

`ll-pica init` 命令用来初始化转换包的配置信息。会读取 `~/.pica/config.json` 文件来生成 package.yaml，

查看 `ll-pica init` 命令的帮助信息：

```bash
ll-pica init --help
```

`ll-pica init` 命令的帮助信息如下：

```bash
init config template

Usage:
  ll-pica init [flags]

Flags:
  -a, --arch string      runtime arch
  -c, --config string    config file
      --dv string        distribution Version
  -h, --help             help for init
      --pi string        package id
      --pn string        package name
  -s, --source string    runtime source
  -t, --type string      get type
  -v, --version string   runtime version
  -w, --workdir string   work directory

Global Flags:
  -V, --verbose   verbose output
```

具体命令如下：

```bash
ll-pica init -w w --pi com.baidu.baidunetdisk --pn com.baidu.baidunetdisk -t repo
```

- -w 表示工作目录
- --pi 指定玲珑应用使用的 appid。
- --pn 指定 apt 能搜索到的正确包名。
- -t 指定获取类型，repo 从仓库中获取。

在 w 目录下会生成 package.yaml 文件。

具体配置如下：

```bash
runtime:
  version: 23.0.1
  base_version: 23.0.0
  source: https://community-packages.deepin.com/beige/
  distro_version: beige
  arch: amd64
file:
  deb:
    - type: repo
      id: com.baidu.baidunetdisk
      name: com.baidu.baidunetdisk
```

详细字段参考：[转换配置文件简介](../manifests.md)。

:::tip
默认使用的 `~/.pica/config.json` 使用的是 deepin v23，如果需要指定 UOS 20 版本作为 BASE 和 RUNTIME 使用以下命令修改默认配置。
下面的 <https://professional-packages.chinauos.com/desktop-professional> 请修改成不需要鉴权的地址。
:::

```bash
ll-pica init --rv "20.0.0" --bv "20.0.0" -s "https://professional-packages.chinauos.com/desktop-professional" --dv "eagle/1070"
```

如果需要在 arm64 上使用，需要修改默认架构。

```bash
ll-pica init -a "arm64"
```
