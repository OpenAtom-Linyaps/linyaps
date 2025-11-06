% ll-builder-push 1

## NAME

ll-builder-push - 推送如意玲珑应用到远程仓库

## SYNOPSIS

**ll-builder push** [*options*]

## DESCRIPTION

`ll-builder push` 命令用来将如意玲珑软件包推送至如意玲珑远程仓库。命令根据项目配置自动推送所有模块或指定的模块到远程仓库。

## OPTIONS

**-h, --help**
: 打印帮助信息并退出

**--help-all**
: 展开所有帮助

**-f, --file** _file_ [./linglong.yaml]
: linglong.yaml 的文件路径

**--repo-url** _url_
: 远程仓库 URL

**--repo-name** _name_
: 远程仓库名

**--module** _module_
: 推送单个模块

## 推送模块说明

当不指定 `--module` 参数时，命令会自动推送项目中的所有模块，包括：

- `binary` - 二进制模块
- `develop` - 开发模块
- 项目中定义的其他自定义模块

当指定 `--module` 参数时，只推送指定的单个模块。

## EXAMPLES

推送项目中的所有模块到远程仓库：

```bash
ll-builder push
```

推送指定模块到远程仓库：

```bash
ll-builder push --module binary
```

推送模块到指定远程仓库：

```bash
ll-builder push --repo-url https://repo.example.com --repo-name myrepo
```

## SEE ALSO

**[ll-builder(1)](./ll-builder.md)**, **[ll-builder-export(1)](export.md)**

## HISTORY

2023年，由 UnionTech Software Technology Co., Ltd. 开发
