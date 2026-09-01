% ll-builder 1

## NAME

ll-builder - 如意玲珑应用构建工具

## SYNOPSIS

**ll-builder** [*options*] _subcommand_

## DESCRIPTION

ll-builder 是为应用开发者提供的一款构建如意玲珑应用的工具。它支持在独立容器内构建应用，并包含完整的推送发布流程。

主要功能包括：

- 在独立容器内构建应用
- 完整的应用构建、测试、导出和发布流程

同一项目工作目录同时只允许执行一个 `build`、`clean`、`run`、`export`、`push`、`generate-lockfile` 或 `update-lockfile` 命令。另一个命令会立即报错，不等待；例如，应用通过 `ll-builder run` 运行期间不能构建或清理同一项目。使用 `-f` 选择同工作目录下的不同配置仍共用项目锁；从不同工作目录使用同一 YAML 入口目录时也会互斥。工作目录和 YAML 入口目录均不重叠的项目可以并行操作。

项目锁使用工作目录和 YAML 入口目录各自的 `.ll-builder.lock`；两个目录相同时只获取一次锁。命令结束或进程退出后自动释放。该文件不应提交到版本控制，`clean` 会保留它；文件存在不代表项目正在被占用，不要在命令执行期间删除它。资源锁定文件 `linglong.lock` 应提交到版本控制。

没有加载 YAML 的命令（例如 `clean`，或目录中没有配置文件时的 `export --ref`）只锁工作目录。

YAML 为文件软链接时，读取链接目标的内容，但资源 lock 写在软链接所在目录，并使用软链接的文件名替换扩展名。例如 `linglong.yaml -> /shared/app.yaml` 对应入口旁的 `linglong.lock`。自动查找与显式 `-f` 使用相同规则；父目录的软链接会先解析为实际目录。

## OPTIONS

**-h, --help**
: 打印帮助信息并退出

**--help-all**
: 展开所有帮助

**--version**
: 显示版本信息

## COMMANDS

| Command | Man Page                              | Description                       |
| ------- | ------------------------------------- | --------------------------------- |
| create  | [ll-builder-create(1)](./create.md)   | 创建如意玲珑构建模板              |
| build   | [ll-builder-build(1)](./build.md)     | 构建如意玲珑项目                  |
| generate-lockfile | [ll-builder-generate-lockfile(1)](./generate-lockfile.md) | 获取资源并生成 lock，不执行项目构建脚本 |
| update-lockfile | [ll-builder-update-lockfile(1)](./update-lockfile.md) | 重新解析资源并更新已有 lock |
| run     | [ll-builder-run(1)](./run.md)         | 运行构建好的应用程序              |
| list    | [ll-builder-list(1)](./list.md)       | 列出已构建的应用程序              |
| remove  | [ll-builder-remove(1)](./remove.md)   | 删除已构建的应用程序              |
| export  | [ll-builder-export(1)](./export.md)   | 导出如意玲珑 layer 或 UAB 文件    |
| push    | [ll-builder-push(1)](./push.md)       | 推送如意玲珑应用到远程仓库        |
| import  | [ll-builder-import(1)](./import.md)   | 导入如意玲珑 layer 文件到构建仓库 |
| extract | [ll-builder-extract(1)](./extract.md) | 将如意玲珑 layer 文件解压到目录   |
| repo    | [ll-builder-repo(1)](./repo.md)       | 显示和管理仓库                    |

## SEE ALSO

**[ll-builder-generate-lockfile(1)](./generate-lockfile.md)**, **[ll-builder-update-lockfile(1)](./update-lockfile.md)**, **[ll-builder-create(1)](./create.md)**, **[ll-builder-build(1)](./build.md)**, **[ll-builder-run(1)](./run.md)**, **[ll-builder-export(1)](./export.md)**, **[ll-builder-push(1)](./push.md)**

## HISTORY

2023年，由 UnionTech Software Technology Co., Ltd. 开发
