% ll-flatpak-convert-convert 1

## NAME

ll\-flatpak\-convert\-convert - 将 Flatpak 应用转换为玲珑应用

## SYNOPSIS

**ll-flatpak-convert convert** [*flags*]

## DESCRIPTION

ll-flatpak-convert convert 命令用于将 Flatpak 应用转换为玲珑应用。该命令使用 ostree 命令拉取 Flatpak 应用数据，根据 metadata 中定义的 runtime 对应生成玲珑的 base 环境。

转换应用默认生成 uab 文件。要转换出 layer 文件，需要添加 --layer 参数。

## OPTIONS

**--build**
: 构建玲珑包

**--layer**
: 导出 layer 文件

**--version** *string*
: 指定生成玲珑应用的版本

**--base** *string*
: 指定玲珑应用的 base 环境

**--base-version** *string*
: 指定玲珑应用的 base 版本

## EXAMPLES

转换 Flatpak 应用 org.videolan.VLC：

```bash
ll-flatpak-convert convert org.videolan.VLC --build
```

转换应用并生成 layer 文件：

```bash
ll-flatpak-convert convert org.videolan.VLC --build --layer
```

指定生成玲珑应用的版本：

```bash
ll-flatpak-convert convert org.videolan.VLC --version "1.0.0.0" --build --layer
```

指定玲珑应用的 base 环境和版本：

```bash
ll-flatpak-convert convert org.videolan.VLC --base "org.deepin.base.flatpak.kde" --base-version "6.7.0.2" --build --layer
```

## SEE ALSO

**[ll-flatpak-convert(1)](ll-flatpak-convert.md)**, **[ll-builder(1)](../ll-builder/ll-builder.md)**

## HISTORY

2024年，由 UnionTech Software Technology Co., Ltd. 开发
