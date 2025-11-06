% ll-builder-extract 1

## NAME

ll-builder-extract - 将如意玲珑 layer 文件解压到目录

## SYNOPSIS

**ll-builder extract** _file_ _directory_

## DESCRIPTION

`ll-builder extract` 命令用于将如意玲珑 layer 文件解压到指定目录。

## OPTIONS

**-h, --help**
: 打印帮助信息并退出

**--help-all**
: 展开所有帮助

**file** (必需)
: 要解压的 layer 文件路径

**directory** (必需)
: 解压目标目录

## EXAMPLES

将 layer 文件解压到指定目录：

```bash
ll-builder extract org.deepin.demo_binary.layer /tmp/extracted
```

## SEE ALSO

**[ll-builder(1)](./ll-builder.md)**, **[ll-builder-export(1)](export.md)**

## HISTORY

2023年，由 UnionTech Software Technology Co., Ltd. 开发
