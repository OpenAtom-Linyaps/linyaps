% ll-pica-init 1

## NAME

ll\-pica\-init - 初始化转换包的配置信息

## SYNOPSIS

**ll-pica init** [*flags*]

## DESCRIPTION

ll-pica init 命令用来初始化转换包的配置信息。该命令会读取 ~/.pica/config.json 文件来生成 package.yaml 配置文件，该文件是 ll-pica 转换 deb 包的基础信息，包含构建的 base、runtime 的版本，以及需要被转换的 deb 包信息。

## OPTIONS

**-a, --arch** _string_
: 运行时架构

**-c, --config** _string_
: 配置文件

**--dv** _string_
: 发行版版本

**--pi** _string_
: 包 ID

**--pn** _string_
: 包名称

**-s, --source** _string_
: 运行时源

**-t, --type** _string_
: 获取类型

**-v, --version** _string_
: 运行时版本

**-w, --workdir** _string_
: 工作目录

**-V, --verbose**
: 详细输出

## EXAMPLES

从仓库中获取包信息：

```bash
ll-pica init -w w --pi com.baidu.baidunetdisk --pn com.baidu.baidunetdisk -t repo
```

指定 UOS 20 版本作为 BASE 和 RUNTIME：

```bash
ll-pica init --rv "20.0.0" --bv "20.0.0" -s "https://professional-packages.chinauos.com/desktop-professional" --dv "eagle/1070"
```

在 arm64 架构上使用：

```bash
ll-pica init -a "arm64"
```

## NOTES

package.yaml 文件生成后，用户可以根据需要进行修改。详细字段参考：[转换配置文件简介](./ll-pica-manifests.md)。

## SEE ALSO

**[ll-pica(1)](ll-pica.md)**, **[ll-builder(1)](ll-pica-convert.md)**

## HISTORY

2024年，由 UnionTech Software Technology Co., Ltd. 开发
