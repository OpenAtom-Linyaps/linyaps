% ll-builder-remove 1

## NAME

ll-builder-remove - 删除已构建的应用程序

## SYNOPSIS

**ll-builder remove** [*options*] _package_

## DESCRIPTION

`ll-builder remove` 命令用于删除已构建的应用程序。

## OPTIONS

**-h, --help**
: 打印帮助信息并退出

**--help-all**
: 展开所有帮助

**--no-clean-objects**
: 删除应用前不清理对象文件

**package** (必需)
: 要删除的应用程序包名

## EXAMPLES

删除指定的应用程序：

```bash
ll-builder remove org.deepin.demo
```

## SEE ALSO

**[ll-builder(1)](./ll-builder.md)**, **[ll-builder-list(1)](list.md)**

## HISTORY

2023年，由 UnionTech Software Technology Co., Ltd. 开发
