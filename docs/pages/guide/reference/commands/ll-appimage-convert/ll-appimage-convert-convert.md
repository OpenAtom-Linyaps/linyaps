% ll-appimage-convert-convert 1

## NAME

ll\-appimage\-convert\-convert - 将 AppImage 包转换为如意玲珑包格式

## SYNOPSIS

**ll-appimage-convert convert** [*flags*]

## DESCRIPTION

ll-appimage-convert convert 命令根据指定的应用名称生成一个目录，该目录会作为如意玲珑项目的根目录，即 linglong.yaml 文件所在的位置。它支持两种转换方法：

1. 使用 `--file` 选项将指定的 appimage 文件转换为如意玲珑包文件
2. 使用 `--url` 和 `--hash` 选项将指定的 appimage url 和 hash 值转换为如意玲珑包文件
3. 使用 `--layer` 选项导出 .layer 格式文件，否则将默认导出 .uab 格式文件

在如意玲珑版本大于1.5.7时，convert 默认导出 uab 包，如果想要导出 layer 文件，需要加上 --layer 参数。

可以使用 `--output` 选项生成如意玲珑项目的配置文件(linglong.yaml)和构建如意玲珑 .layer (.uab)的脚本文件，然后可以执行该脚本去生成对应的如意玲珑包当修改 linglong.yaml 配置文件后。如果不指定该选项，将直接导出对应的如意玲珑包。

## OPTIONS

**-b, --build**
: 构建玲珑包

**-d, --description** _string_
: 包的详细描述

**-f, --file** _string_
: app 包文件，当设置了 --url 选项和 --hash 选项时，此选项不是必需的

**--hash** _string_
: 包哈希值，必须与 --url 选项一起使用

**-i, --id** _string_
: 包的唯一名称

**-l, --layer**
: 导出 layer 文件

**-n, --name** _string_
: 包的描述名称

**-u, --url** _string_
: 包 URL，当设置了 -f 选项时，此选项不是必需的

**-v, --version** _string_
: 包的版本

**-V, --verbose**
: 详细输出

## EXAMPLES

通过 --url 选项将 BrainWaves appimage 文件转换为如意玲珑 .layer 文件：

```bash
ll-appimage-convert convert --url "https://github.com/makebrainwaves/BrainWaves/releases/download/v0.15.1/BrainWaves-0.15.1.AppImage" --hash "04fcfb9ccf5c0437cd3007922fdd7cd1d0a73883fd28e364b79661dbd25a4093" --name "io.github.brainwaves" --id "io.github.brainwaves" --version "0.15.1.0" --description "io.github.brainwaves" -b
```

通过 --file 选项将 BrainWaves-0.15.1.AppImage 转换为如意玲珑 .uab：

```bash
ll-appimage-convert convert -f ~/Downloads/BrainWaves-0.15.1.AppImage --name "io.github.brainwaves" --id "io.github.brainwaves" --version "0.15.1.0" --description "io.github.brainwaves" -b
```

## SEE ALSO

**[ll-appimage-convert(1)](ll-appimage-convert.md)**, **[ll-builder(1)](../ll-builder/ll-builder.md)**

## HISTORY

2024年，由 UnionTech Software Technology Co., Ltd. 开发
