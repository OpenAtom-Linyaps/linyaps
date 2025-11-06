% ll-pica-convert 1

## NAME

ll\-pica\-convert - 将 deb 包转换为玲珑包

## SYNOPSIS

**ll-pica convert** [*flags*]

## DESCRIPTION

ll-pica convert 命令用来生成玲珑需要使用的 linglong.yaml 文件，并将 deb 包转换为玲珑包。该命令支持从本地 deb 文件或通过 apt download 命令从仓库下载 deb 包进行转换。

## OPTIONS

**-b, --build**
: 构建玲珑包

**-c, --config** _string_
: 配置文件

**--exportFile** _string_
: 导出 uab 或 layer（默认为 "uab"）

**--pi** _string_
: 包 ID

**--pn** _string_
: 包名称

**-t, --type** _string_
: 获取应用类型（默认为 "local"）

**--withDep**
: 添加依赖树

**-w, --workdir** _string_
: 工作目录

**-V, --verbose**
: 详细输出

## EXAMPLES

从仓库下载 deb 包并转换：

```bash
ll-pica init -w w --pi com.baidu.baidunetdisk --pn com.baidu.baidunetdisk -t repo
```

```bash
ll-pica convert -w w -b --exportFile
```

从本地 deb 文件转换：

```bash
ll-pica convert -c com.baidu.baidunetdisk_4.17.7_amd64.deb -w w -b --exportFile layer
```

## SEE ALSO

**[ll-pica(1)](ll-pica.md)**, **[ll-cli(1)](ll-pica-init.md)**

## HISTORY

2024年，由 UnionTech Software Technology Co., Ltd. 开发
