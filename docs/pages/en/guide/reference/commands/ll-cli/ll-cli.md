% ll-cli 1

## NAME

ll\-cli - Linyaps command-line tool for managing applications and runtimes

## SYNOPSIS

**ll-cli** [*global-options*] _subcommand_ [*subcommand-options*]

## DESCRIPTION

ll-cli is a package manager frontend for managing Linyaps application installation, uninstallation, viewing, launching, termination, debugging, updating, and other operations. It provides complete application lifecycle management functionality.

## GLOBAL OPTIONS

**-h, --help**
: Print help information and exit

**--help-all**
: Expand all help information

**--version**
: Display version information

**-v, --verbose**
: Show debug information (detailed logs)

**--json**
: Output results in JSON format

**--no-dbus**
: Use peer-to-peer D-Bus, only used when D-Bus daemon is unavailable (internal use)

## COMMANDS

| Command   | Man Page                              | Description                                                                             |
| --------- | ------------------------------------- | --------------------------------------------------------------------------------------- |
| run       | [ll-cli-run(1)](./run.md)             | Run applications                                                                        |
| ps        | [ll-cli-ps(1)](./ps.md)               | List running applications                                                               |
| enter     | [ll-cli-exec(1)](./enter.md)          | Enter the namespace of a running application                                            |
| kill      | [ll-cli-kill(1)](./kill.md)           | Stop running applications                                                               |
| prune     | [ll-cli-prune(1)](./prune.md)         | Remove unused base systems or runtimes                                                  |
| install   | [ll-cli-install(1)](./install.md)     | Install applications or runtimes                                                        |
| uninstall | [ll-cli-uninstall(1)](./uninstall.md) | Uninstall applications or runtimes                                                      |
| upgrade   | [ll-cli-upgrade(1)](./upgrade.md)     | Upgrade applications or runtimes                                                        |
| list      | [ll-cli-list(1)](./list.md)           | List installed applications or runtimes                                                 |
| info      | [ll-cli-info(1)](./info.md)           | Display information about installed applications or runtimes                            |
| content   | [ll-cli-content(1)](./content.md)     | Display exported files of installed applications                                        |
| search    | [ll-cli-search(1)](./search.md)       | Search for applications/runtimes containing specified keywords from remote repositories |
| repo      | [ll-cli-repo(1)](./repo.md)           | Display or modify current repository information                                        |

## SEE ALSO

**[ll-cli-run(1)](./run.md)**, **[ll-cli-ps(1)](./ps.md)**, **[ll-cli-exec(1)](./enter.md)**, **[ll-cli-kill(1)](./kill.md)**, **[ll-cli-prune(1)](./prune.md)**, **[ll-cli-install(1)](./install.md)**, **[ll-cli-uninstall(1)](./uninstall.md)**, **[ll-cli-upgrade(1)](./upgrade.md)**, **[ll-cli-list(1)](./list.md)**, **[ll-cli-info(1)](./info.md)**, **[ll-cli-content(1)](./content.md)**, **[ll-cli-search(1)](./search.md)**, **[ll-cli-repo(1)](./repo.md)**

## HISTORY

Developed in 2023 by UnionTech Software Technology Co., Ltd.
