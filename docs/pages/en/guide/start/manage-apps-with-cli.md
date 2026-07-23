<!--
SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# Manage Applications with the CLI

This chapter assumes that you have completed the [Quick Start](./quick-start.md) and can use `search`, `install`, and `run` to search for, install, and run applications. It covers two areas:

- Managing installed applications, including inspecting, upgrading, and uninstalling them.
- Managing running applications, including inspecting and stopping them.

## Manage Installed Applications

### Install a Specific Application Version

First, query all versions available in the repository:

```bash
ll-cli search org.deepin.calculator --show-all-version
```

Append a version to the application ID to install that version:

```bash
ll-cli install org.deepin.calculator/6.5.26.1
```

See [`ll-cli search`](../reference/commands/ll-cli/search.md) and [`ll-cli install`](../reference/commands/ll-cli/install.md) for all options.

### List Installed Applications

Use the following command if you have forgotten an application ID, need to confirm that an application is installed, or want to check the installed version:

```bash
ll-cli list
```

To show only applications that can be upgraded:

```bash
ll-cli list --upgradable
```

You can also filter the results by App, Runtime, or Base. See [`ll-cli list`](../reference/commands/ll-cli/list.md) for all options.

### Upgrade Applications

Upgrade all installed applications:

```bash
ll-cli upgrade
```

Upgrade one application:

```bash
ll-cli upgrade org.deepin.calculator
```

See [`ll-cli upgrade`](../reference/commands/ll-cli/upgrade.md) for all options.

### Uninstall an Application

```bash
ll-cli uninstall org.deepin.calculator
```

See [`ll-cli uninstall`](../reference/commands/ll-cli/uninstall.md) for all options.

## Manage Running Applications

### List Running Applications

Use the following command to check whether an application is still running or to diagnose duplicate launches and processes that remain after their windows disappear:

```bash
ll-cli ps
```

The output includes the application ID, container ID, and process ID. See [`ll-cli ps`](../reference/commands/ll-cli/ps.md) for all options.

### Stop a Running Application

If an application is unresponsive, cannot exit normally, or must be stopped before an upgrade, first confirm its ID with `ll-cli ps`, then run:

```bash
ll-cli kill org.deepin.calculator
```

This sends `SIGTERM` by default. If the application still does not exit, send signal 9 (`SIGKILL`) to terminate it forcibly:

```bash
ll-cli kill -s 9 org.deepin.calculator
```

Forced termination does not give the application an opportunity to save data or clean up resources and can cause unsaved data to be lost. Use it only when a normal `kill` does not work. See [`ll-cli kill`](../reference/commands/ll-cli/kill.md) for all options.

## View Command Help

When you are unsure which subcommands or options are available, use `--help`. To view the global options and subcommand list for `ll-cli`:

```bash
ll-cli --help
```

Place `--help` after a subcommand to view its options and usage. For example:

```bash
ll-cli install --help
```

Other subcommands work the same way:

```bash
ll-cli run --help
ll-cli list --help
ll-cli kill --help
```

If the regular help hides advanced options, use `--help-all`:

```bash
ll-cli --help-all
ll-cli run --help-all
```

## Continue Reading

- The [`ll-cli` Command Reference](../reference/commands/ll-cli/ll-cli.md) lists every subcommand and its detailed usage.
- [Manage Runtimes with the CLI](./manage-runtimes-with-cli.md) covers dependency and disk-usage analysis, removal of unused runtimes, and forced operations.
- The [FAQ](../tips-and-faq/faq.md) covers common issues involving application execution, data directories, desktop integration, and dependencies.
- When diagnosing a problem, add `--verbose` after the command to obtain detailed logs. Provide the community with the complete command, logs, system version, and output of `ll-cli --version`.
