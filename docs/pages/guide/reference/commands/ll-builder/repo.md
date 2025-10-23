% ll-builder-repo 1

## NAME

ll-builder-repo - 显示和管理仓库

## SYNOPSIS

**ll-builder repo** [*options*] *subcommand*

## DESCRIPTION

`ll-builder repo` 命令用于显示和管理如意玲珑仓库。

## OPTIONS

**-h, --help**
: 打印帮助信息并退出

**--help-all**
: 展开所有帮助

## SUBCOMMANDS

**show**
: 显示仓库信息

**add** *name* *url* [*options*]
: 添加新的仓库配置

**remove** *alias*
: 移除仓库配置

**update** *alias* *url*
: 更新仓库 URL

**set-default** *alias*
: 设置默认仓库名称

**enable-mirror** *alias*
: 启用仓库镜像

**disable-mirror** *alias*
: 禁用仓库镜像

## EXAMPLES

显示仓库信息：

```bash
ll-builder repo show
```

添加新的仓库：

```bash
ll-builder repo add myrepo https://repo.example.com
```

添加仓库并设置别名：

```bash
ll-builder repo add myrepo https://repo.example.com --alias myalias
```

移除仓库：

```bash
ll-builder repo remove myalias
```

更新仓库 URL：

```bash
ll-builder repo update myalias https://new-repo.example.com
```

设置默认仓库：

```bash
ll-builder repo set-default myalias
```

启用仓库镜像：

```bash
ll-builder repo enable-mirror myalias
```

禁用仓库镜像：

```bash
ll-builder repo disable-mirror myalias
```

## SEE ALSO

**[ll-builder(1)](./ll-builder.md)**

## HISTORY

2023年，由 UnionTech Software Technology Co., Ltd. 开发
