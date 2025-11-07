% ll-cli 1

## NAME

ll\-cli - 如意玲珑命令行工具，用于管理应用程序和运行时

## SYNOPSIS

**ll-cli** [*global-options*] _subcommand_ [*subcommand-options*]

## DESCRIPTION

ll-cli 是一个包管理器前端，用于管理如意玲珑应用的安装、卸载、查看、启动、关闭、调试、更新等操作。它提供了完整的应用程序生命周期管理功能。

## GLOBAL OPTIONS

**-h, --help**
: 打印帮助信息并退出

**--help-all**
: 展开所有帮助

**--version**
: 显示版本信息

**-v, --verbose**
: 显示调试信息（详细日志）

**--json**
: 使用JSON格式输出结果

**--no-dbus**
: 使用点对点DBus，仅在DBus守护进程不可用时使用（内部使用）

## COMMANDS

| Command   | Man Page                              | Description                                     |
| --------- | ------------------------------------- | ----------------------------------------------- |
| run       | [ll-cli-run(1)](./run.md)             | 运行应用程序                                    |
| ps        | [ll-cli-ps(1)](./ps.md)               | 列出正在运行的应用程序                          |
| enter     | [ll-cli-exec(1)](./enter.md)          | 进入应用程序正在运行的命名空间                  |
| kill      | [ll-cli-kill(1)](./kill.md)           | 停止运行的应用程序                              |
| prune     | [ll-cli-prune(1)](./prune.md)         | 移除未使用的最小系统或运行时                    |
| install   | [ll-cli-install(1)](./install.md)     | 安装应用程序或运行时                            |
| uninstall | [ll-cli-uninstall(1)](./uninstall.md) | 卸载应用程序或运行时                            |
| upgrade   | [ll-cli-upgrade(1)](./upgrade.md)     | 升级应用程序或运行时                            |
| list      | [ll-cli-list(1)](./list.md)           | 列出已安装的应用程序或运行时                    |
| info      | [ll-cli-info(1)](./info.md)           | 显示已安装的应用程序或运行时的信息              |
| content   | [ll-cli-content(1)](./content.md)     | 显示已安装应用导出的文件                        |
| search    | [ll-cli-search(1)](./search.md)       | 从远程仓库中搜索包含指定关键词的应用程序/运行时 |
| repo      | [ll-cli-repo(1)](./repo.md)           | 显示或修改当前使用的仓库信息                    |

## SEE ALSO

**[ll-cli-run(1)](./run.md)**, **[ll-cli-ps(1)](./ps.md)**, **[ll-cli-exec(1)](./enter.md)**, **[ll-cli-kill(1)](./kill.md)**, **[ll-cli-prune(1)](./prune.md)**, **[ll-cli-install(1)](./install.md)**, **[ll-cli-uninstall(1)](./uninstall.md)**, **[ll-cli-upgrade(1)](./upgrade.md)**, **[ll-cli-list(1)](./list.md)**, **[ll-cli-info(1)](./info.md)**, **[ll-cli-content(1)](./content.md)**, **[ll-cli-search(1)](./search.md)**, **[ll-cli-repo(1)](./repo.md)**

## HISTORY

2023年，由 UnionTech Software Technology Co., Ltd. 开发
