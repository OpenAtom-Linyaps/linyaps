% ll-cli-prune 1

## NAME

ll\-cli\-prune - 移除未使用的最小系统或运行时

## SYNOPSIS

**ll-cli prune** [*options*]

## DESCRIPTION

使用 `ll-cli prune` 移除未使用的最小系统或运行时。该命令用于清理系统中不再被任何应用程序使用的基础环境或运行时组件，释放磁盘空间。

## OPTIONS

**-h, --help**
: 打印帮助信息并退出

**--help-all**
: 展开所有帮助

## EXAMPLES

使用 `ll-cli prune` 移除未使用的最小系统或运行时：

```bash
ll-cli prune
```

`ll-cli prune` 输出如下:

```text
Unused base or runtime:
main:org.deepin.Runtime/23.0.1.2/x86_64
main:org.deepin.foundation/20.0.0.27/x86_64
main:org.deepin.Runtime/23.0.1.0/x86_64
main:org.deepin.foundation/23.0.0.27/x86_64
main:org.deepin.Runtime/20.0.0.8/x86_64
5 unused base or runtime have been removed.
```

## SEE ALSO

**[ll-cli(1)](./ll-cli.md)**, **[ll-cli-list(1)](./list.md)**, **[ll-cli-uninstall(1)](./uninstall.md)**

## HISTORY

2023年，由 UnionTech Software Technology Co., Ltd. 开发
