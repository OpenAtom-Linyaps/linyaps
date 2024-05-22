## 转换应用

`ll-pica convert` 命令用来生成玲珑需要使用的 linglong.yaml 文件。

查看 `ll-pica convert` 命令的帮助信息：

```bash
ll-pica convert --help
```

`ll-pica convert` 命令的帮助信息如下：

```bash
Convert deb to uab

Usage:
  ll-pica convert [flags]

Flags:
  -b, --build            build linglong
  -c, --config string    config file
  -h, --help             help for convert
      --pi string        package id
      --pn string        package name
  -t, --type string      get app type (default "local")
  -w, --workdir string   work directory

Global Flags:
  -V, --verbose   verbose output
```

在执行  `ll-pica init -w w --pi com.baidu.baidunetdisk --pn com.baidu.baidunetdisk -t repo` 命令后，我们仅需要执行 `ll-pica convert -w w -b` 命令来转换出玲珑应用，这里会使用 `apt download` 命令去下载包名为 `com.baidu.baidunetdisk` 的 deb 包。

:::tip
这里使用 apt download 命令下载 deb 包，可能由于 deb 包过大而下载或者无法获取链接导致，失败推荐使用下面的命令。

如果直接使用下面的命令，就不需要执行 `ll-pica init -w w --pi com.baidu.baidunetdisk --pn com.baidu.baidunetdisk -t repo` 命令
:::

```bash
apt download com.baidu.baidunetdisk
```

```bash
ll-pica convert -c com.baidu.baidunetdisk_4.17.7_amd64.deb -w w -b
```

- -w 表示工作目录。
- -c 配置的方式，这里使用 deb 文件。
- -b 表示需要进行构建，不添加该参数不会进行构建和导出 layer 文件。

构建产物如下：

```bash
├── package
│   └── com.baidu.baidunetdisk
│       ├── com.baidu.baidunetdisk_4.17.7.0_x86_64_develop.layer
│       ├── com.baidu.baidunetdisk_4.17.7.0_x86_64_runtime.layer
│       ├── linglong
│       └── linglong.yaml
└── package.yaml
```

目前玲珑应用的产物是 layer 文件，layer 文件分为runtime 和 develop, runtime 包含应用的运行环境，develop 在 runtime 的基础上保留调试环境。

安装 layer 文件和运行应用参考：[安装应用](../ll-cli/install.md)
