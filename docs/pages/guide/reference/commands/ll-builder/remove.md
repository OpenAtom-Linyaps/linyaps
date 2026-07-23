% ll-builder-remove 1

## NAME

ll-builder-remove - 删除已构建的应用程序

## SYNOPSIS

**ll-builder remove** [*options*] _ref_...

## DESCRIPTION

`ll-builder remove` 命令用于从本地构建仓库删除一个或多个构建结果。每个构建结果必须使用 `ll-builder list` 输出的完整 ref 指定。

## OPTIONS

**-h, --help**
: 打印帮助信息并退出

**--help-all**
: 展开所有帮助

**--no-clean-objects**
: 删除应用前不清理对象文件

**REF** _TEXT_... _REQUIRED_
: 要删除的完整软件包引用，可指定多个。格式为 `channel:id/version/arch`

## EXAMPLES

删除指定的构建结果：

```bash
ll-builder remove main:org.deepin.demo/0.0.0.1/x86_64
```

## SEE ALSO

**[ll-builder(1)](./ll-builder.md)**, **[ll-builder-list(1)](list.md)**

## HISTORY

2023年，由 UnionTech Software Technology Co., Ltd. 开发
