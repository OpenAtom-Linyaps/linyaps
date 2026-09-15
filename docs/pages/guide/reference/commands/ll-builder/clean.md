% ll-builder-clean 1

## NAME

ll-builder-clean - 清理构建产物

## SYNOPSIS

**ll-builder clean** [*options*]

## DESCRIPTION

`ll-builder clean` 命令用于清理上一次构建生成的构建产物，这些产物存放在当前工作目录的 `linglong/` 目录中。

如果 `linglong/` 目录不存在，该命令会直接成功返回，不做任何操作。

## OPTIONS

**-h, --help**
: 打印帮助信息并退出

**--help-all**
: 展开所有帮助

**-f, --file** _FILE_
: 指定当前工作目录下的项目配置文件。省略时优先使用 `linglong.<当前架构>.yaml`（如果存在），否则使用 `linglong.yaml`

## EXAMPLES

清理当前工作目录下项目的构建产物：

```bash
ll-builder clean
```

显式指定项目配置文件：

```bash
ll-builder clean --file linglong.custom.yaml
```

## SEE ALSO

**[ll-builder(1)](./ll-builder.md)**

## HISTORY

2023年，由 UnionTech Software Technology Co., Ltd. 开发
