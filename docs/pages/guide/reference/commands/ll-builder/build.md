% ll-builder-build 1

## NAME

ll-builder-build - 构建如意玲珑项目

## SYNOPSIS

**ll-builder build** [*options*] [*command*...]

## DESCRIPTION

`ll-builder build` 命令用来构建如意玲珑应用。构建完成后，构建内容将自动提交到本地 `ostree` 缓存中。

## OPTIONS

**-h, --help**
: 打印帮助信息并退出

**--help-all**
: 展开所有帮助

**-f, --file** _file_ [./linglong.yaml]
: linglong.yaml 的文件路径

**--offline**
: 仅使用本地文件。这意味着将设置 --skip-fetch-source 和 --skip-pull-depend

**--skip-fetch-source**
: 跳过获取源代码

**--skip-pull-depend**
: 跳过拉取依赖项

**--skip-run-container**
: 跳过运行容器

**--skip-commit-output**
: 跳过提交构建输出

**--skip-output-check**
: 跳过输出检查

**--skip-strip-symbols**
: 跳过剥离调试符号

**--isolate-network**
: 在隔离的网络环境中构建

**command** -- _COMMAND_ ...
: 进入容器执行命令而不是构建应用

## EXAMPLES

### 基本用法

在项目根目录（linglong.yaml 文件所在目录）构建应用：

```bash
ll-builder build
```

或使用 `--file` 参数指定配置文件路径：

```bash
ll-builder build --file /path/to/linglong.yaml
```

进入容器执行命令而不是构建应用：

```bash
ll-builder build -- bash
```

## SEE ALSO

**[ll-builder(1)](./ll-builder.md)**, **[ll-builder-export(1)](export.md)**

## HISTORY

2023年，由 UnionTech Software Technology Co., Ltd. 开发
