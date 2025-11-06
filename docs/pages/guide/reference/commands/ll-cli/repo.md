% ll-cli-repo 1

## NAME

ll\-cli\-repo - 显示或修改当前使用的仓库信息

## SYNOPSIS

**ll-cli repo** _subcommand_ [*options*]

## DESCRIPTION

`ll-cli repo` 命令用于管理远程仓库。该命令提供了一系列子命令来添加、修改、删除、显示和配置仓库信息。

## SUBCOMMANDS

### add

添加一个新的仓库。

**用法**: `ll-cli repo add [OPTIONS] NAME URL`

**参数**:

- **NAME** _TEXT_ _REQUIRED_: 指定仓库名称
- **URL** _TEXT_ _REQUIRED_: 仓库的URL地址
- **--alias** _ALIAS_: 仓库名称的别名

**示例**:

```bash
ll-cli repo add myrepo https://repo.example.com
```

### remove

删除一个仓库。

**用法**: `ll-cli repo remove [OPTIONS] NAME`

**参数**:

- **ALIAS** _TEXT_ _REQUIRED_: 仓库名称的别名

**示例**:

```bash
ll-cli repo remove myrepo
```

### update

更新仓库URL。

**用法**: `ll-cli repo update [OPTIONS] NAME URL`

**参数**:

- **ALIAS** _TEXT_ _REQUIRED_: 仓库名称的别名
- **URL** _TEXT_ _REQUIRED_: 新的仓库URL地址

**示例**:

```bash
ll-cli repo update myrepo https://updated-repo.example.com
```

### set-default

设置默认仓库名称。

**用法**: `ll-cli repo set-default [OPTIONS] NAME`

**参数**:

- **Alias** _TEXT_ _REQUIRED_: 仓库名称的别名

**示例**:

```bash
ll-cli repo set-default myrepo
```

### show

显示仓库信息。

**用法**: `ll-cli repo show [OPTIONS]`

**示例**:

```bash
ll-cli repo show
```

### set-priority

设置仓库优先级。

**用法**: `ll-cli repo set-priority ALIAS PRIORITY`

**参数**:

- **ALIAS** _TEXT_ _REQUIRED_: 仓库名称的别名
- **PRIORITY** _TEXT_ _REQUIRED_: 仓库的优先级

**示例**:

```bash
ll-cli repo set-priority myrepo 100
```

### enable-mirror

为仓库启用镜像。

**用法**: `ll-cli repo enable-mirror [OPTIONS] ALIAS`

**参数**:

- **ALIAS** _TEXT_ _REQUIRED_: 仓库名称的别名

**示例**:

```bash
ll-cli repo enable-mirror myrepo
```

### disable-mirror

为仓库禁用镜像。

**用法**: `ll-cli repo disable-mirror [OPTIONS] ALIAS`

**参数**:

- **ALIAS** _TEXT_ _REQUIRED_: 仓库名称的别名

**示例**:

```bash
ll-cli repo disable-mirror myrepo
```

## OPTIONS

**-h, --help**
: 打印帮助信息并退出

**--help-all**
: 展开所有帮助

## EXAMPLES

显示所有仓库信息：

```bash
ll-cli repo show
```

添加一个新的仓库：

```bash
ll-cli repo add deepin https://repo.deepin.com
```

更新仓库URL：

```bash
ll-cli repo update deepin https://new-repo.deepin.com
```

设置仓库优先级：

```bash
ll-cli repo set-priority deepin 50
```

移除一个仓库：

```bash
ll-cli repo remove deepin
```

## SEE ALSO

**[ll-cli(1)](./ll-cli.md)**, **[ll-cli-search(1)](./search.md)**, **[ll-cli-install(1)](./install.md)**

## HISTORY

2023年，由 UnionTech Software Technology Co., Ltd. 开发
