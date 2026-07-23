% ll-cli-analyze 1

## NAME

ll\-cli\-analyze - 分析已安装应用

## SYNOPSIS

**ll-cli analyze** _subcommand_ [*options*]

## DESCRIPTION

`ll-cli analyze` 用于分析已安装模块的磁盘占用和应用依赖关系。必须指定一个子命令。

## SUBCOMMANDS

### size

显示已安装模块的磁盘占用以及仓库的实际占用。

**用法**：`ll-cli analyze size [OPTIONS]`

**--sort** _FIELD_ [*actual*]
: 指定排序字段。可选值为 `actual`、`logical`、`exclusive`、`shared` 或 `id`

**--asc**
: 按升序排列；默认按降序排列

```bash
ll-cli analyze size
ll-cli analyze size --sort exclusive --asc
```

### depends

显示已安装应用的依赖树。不指定应用时显示所有已安装应用的依赖树。

**用法**：`ll-cli analyze depends [APP]`

```bash
ll-cli analyze depends
ll-cli analyze depends org.deepin.demo
```

## OPTIONS

**-h, --help**
: 打印帮助信息并退出

**--help-all**
: 展开所有帮助

## SEE ALSO

**[ll-cli(1)](./ll-cli.md)**, **[ll-cli-list(1)](./list.md)**, **[ll-cli-info(1)](./info.md)**

## HISTORY

2026年，由 UnionTech Software Technology Co., Ltd. 开发
