% ll-builder 1

## NAME

ll-builder - 如意玲珑应用构建工具

## SYNOPSIS

**ll-builder** _subcommand_

## DESCRIPTION

ll-builder 是为应用开发者提供的一款构建如意玲珑应用的工具。它支持在独立容器内构建应用，并包含完整的推送发布流程。

主要功能包括：

- 在独立容器内构建应用
- 完整的应用构建、测试、导出和发布流程

## COMMANDS

| Command | Man Page                              | Description                       |
| ------- | ------------------------------------- | --------------------------------- |
| create  | [ll-builder-create(1)](./create.md)   | 创建如意玲珑构建模板              |
| build   | [ll-builder-build(1)](./build.md)     | 构建如意玲珑项目                  |
| run     | [ll-builder-run(1)](./run.md)         | 运行构建好的应用程序              |
| list    | [ll-builder-list(1)](./list.md)       | 列出已构建的应用程序              |
| remove  | [ll-builder-remove(1)](./remove.md)   | 删除已构建的应用程序              |
| export  | [ll-builder-export(1)](./export.md)   | 导出如意玲珑 layer 或 UAB 文件    |
| push    | [ll-builder-push(1)](./push.md)       | 推送如意玲珑应用到远程仓库        |
| import  | [ll-builder-import(1)](./import.md)   | 导入如意玲珑 layer 文件到构建仓库 |
| extract | [ll-builder-extract(1)](./extract.md) | 将如意玲珑 layer 文件解压到目录   |
| repo    | [ll-builder-repo(1)](./repo.md)       | 显示和管理仓库                    |

## SEE ALSO

**[ll-builder-create(1)](./create.md)**, **[ll-builder-build(1)](./build.md)**, **[ll-builder-run(1)](./run.md)**, **[ll-builder-export(1)](./export.md)**, **[ll-builder-push(1)](./push.md)**

## HISTORY

2023年，由 UnionTech Software Technology Co., Ltd. 开发
