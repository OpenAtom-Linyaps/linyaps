<!--
SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# Quick Start

This chapter is intended for end users of Linyaps applications. After completing the steps below, you will be able to use `ll-cli` to search for, install, and run applications.

## Prepare ll-cli

Some versions of deepin and UOS include `ll-cli`. First, check the installed version in a terminal:

```bash
ll-cli --version
```

If the command is not found, use the following one-line command for a quick installation:

```bash
curl -fsSL https://get.linyaps.org.cn | sh
```

The installer detects the current Linux distribution and requests `sudo` privileges when necessary. After installation, you can begin using Linyaps applications.

Application developers can use the following command to install the `ll-builder` build tool as well:

```bash
curl -fsSL https://get.linyaps.org.cn | sh -s -- full
```

If your distribution is not supported or you need to configure a repository manually, see [Install Linyaps](./install.md).

## Install and Run an Application

Using the Calculator application as an example, first search for applications whose names contain `calculator`:

```bash
ll-cli search calculator
```

The search results contain output similar to:

```
ID                                         Name                             Version         Channel          Module      Repo      Description
...
org.deepin.calculator                      deepin-calculator                6.5.37.1        main             binary      stable    Calculator for UOS
```

The Calculator application ID is `org.deepin.calculator`. Install the application with that ID:

```bash
ll-cli install org.deepin.calculator
```

After installation finishes, run Calculator:

```bash
ll-cli run org.deepin.calculator
```

When the Calculator window appears, you have completed your first experience of installing and running a Linyaps application.

## Continue Reading

* To inspect, stop, upgrade, or uninstall applications, continue with [Manage Applications with the CLI](./manage-apps-with-cli.md).
* To build an application, continue with [Build Your First Linyaps Application](./build_your_first_app.md).
* The [ll-cli](../reference/commands/ll-cli/ll-cli.md) command reference lists all subcommands.
* The [FAQ](../tips-and-faq/faq.md) covers common issues involving application execution, data directories, desktop integration, and dependencies.
