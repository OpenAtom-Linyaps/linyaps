## 转换 Flatpak 应用

`ll-pica-flatpak convert` 命令用来将 Flatpak 应用转换为玲珑应用。

查看 `ll-pica-flatpak convert` 命令的帮助信息：

```bash
ll-pica-flatpak --help
```

`ll-pica-flatpak convert` 命令的帮助信息如下：

```bash
Convert the flatpak to uab. For example:
Simple:
        ll-pica-flatpak convert [flatpak name] --build

Usage:
  ll-pica-flatpak [command]

Available Commands:
  convert     Convert flatpak to uab
  help        Help about any command

Flags:
  -h, --help      help for ll-pica-flatpak
```

转换应用

```bash
ll-pica-flatpak convert org.videolan.VLC --build
```

:::tip

org.videolan.VLC 为 Flatpak 的软件包名。可通过 https://flathub.org/ => 点击应用 => 点击 Install 下拉菜单=> 查看软件包名。

ll-pica-flatpak 使用 ostree 命令拉取 org.videolan.VLC 应用数据，根据 metadata 中定义的 runtime 对应生成玲珑的 base 环境。

:::

转换应用默认生成，uab 文件。转换出 layer 文件，需要添加 --layer 参数。

```bash
ll-pica-flatpak convert org.videolan.VLC --build --layer
```

如果需要指定生成玲珑应用的版本，需要添加 --version 参数。

```bash
ll-pica-flatpak convert org.videolan.VLC --version "1.0.0.0" --build --layer
```

如果需要指定玲珑应用的 base 环境和版本，需要添加 --base 和 --base-version 参数。

```bash
ll-pica-flatpak convert org.videolan.VLC --base "org.deepin.base.flatpak.kde" --base-version "6.7.0.2" --build --layer
```

构建产物如下：

```bash
├── org.videolan.VLC
│   ├── org.videolan.VLC_1.0.0.0_x86_64_develop.layer
│   ├── org.videolan.VLC_1.0.0.0_x86_64_binary.layer
    or
│   ├── org.videolan.VLC_x86_64_1.0.0.0_main.uab
```

玲珑应用的产物 ：

layer 文件，layer 文件分为 binary 和 develop, binary 包含应用的运行环境，develop 在 binary 的基础上保留调试环境。

uab 文件，是玲珑软件包使用的离线分发格式，并不适合可以正常连接到玲珑仓库的系统使用，应当使用玲珑软件仓库提供的增量传输方案以减少网络传输体积。

安装 layer 文件和运行应用参考：[安装应用](../ll-cli/install.md)
