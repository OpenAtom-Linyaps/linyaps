% ll-builder-import 1

## NAME

ll-builder-import - 导入如意玲珑 layer 文件到构建仓库

## SYNOPSIS

**ll-builder import** [*options*] _file_

## DESCRIPTION

`ll-builder import` 命令用于导入如意玲珑 layer 文件到构建仓库。

## OPTIONS

**-h, --help**
: 打印帮助信息并退出

**--help-all**
: 展开所有帮助

**file** (必需)
: 要导入的 layer 文件路径

## EXAMPLES

导入 layer 文件到构建仓库：

```bash
ll-builder import org.deepin.demo_binary.layer
```

## SEE ALSO

**[ll-builder(1)](./ll-builder.md)**, **[ll-builder-export(1)](export.md)**

## HISTORY

2023年，由 UnionTech Software Technology Co., Ltd. 开发
