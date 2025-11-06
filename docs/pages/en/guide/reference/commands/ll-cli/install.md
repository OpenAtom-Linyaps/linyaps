% ll-cli-install 1

## NAME

ll\-cli\-install - Install applications or runtimes

## SYNOPSIS

**ll-cli install** [*options*] _app_

## DESCRIPTION

The `ll-cli install` command is used to install Linyaps applications. This command supports installing applications or runtimes through application names, .layer files, or .uab files.

## OPTIONS

**-h, --help**
: Print help information and exit

**--help-all**
: Expand all help

**--module** _MODULE_
: Install specified module

**--repo** _REPO_
: Install from specified repository

**--force**
: Force installation of specified application version

**-y**
: Automatically answer yes to all questions

## POSITIONAL ARGUMENTS

**APP** _TEXT_ _REQUIRED_
: Specify application name, can also be .uab or .layer file

## EXAMPLES

Install application through application name:

```bash
ll-cli install org.deepin.demo
```

Install application through Linyaps .layer file:

```bash
ll-cli install demo_0.0.0.1_x86_64_binary.layer
```

Install application through Linyaps .uab file:

```bash
ll-cli install demo_x86_64_0.0.0.1_main.uab
```

Install specified module of application:

```bash
ll-cli install org.deepin.demo --module=binary
```

Install specified version of application:

```bash
ll-cli install org.deepin.demo/0.0.0.1
```

Install application through specific identifier:

```bash
ll-cli install stable:org.deepin.demo/0.0.0.1/x86_64
```

## SEE ALSO

**[ll-cli(1)](./ll-cli.md)**, **[ll-cli-uninstall(1)](uninstall.md)**, **[ll-cli-upgrade(1)](upgrade.md)**, **[ll-cli-list(1)](list.md)**

## HISTORY

Developed in 2023 by UnionTech Software Technology Co., Ltd.
