% ll-builder-repo 1

## NAME

ll-builder-repo - Display and manage repositories

## SYNOPSIS

**ll-builder repo** [*options*] *subcommand*

## DESCRIPTION

The `ll-builder repo` command is used to display and manage Linyaps repositories.

## OPTIONS

**-h, --help**
: Print help information and exit

**--help-all**
: Expand all help

## SUBCOMMANDS

**show**
: Display repository information

**add** *name* *url* [*options*]
: Add new repository configuration

**remove** *alias*
: Remove repository configuration

**update** *alias* *url*
: Update repository URL

**set-default** *alias*
: Set default repository name

**enable-mirror** *alias*
: Enable repository mirror

**disable-mirror** *alias*
: Disable repository mirror

## EXAMPLES

Display repository information:

```bash
ll-builder repo show
```

Add a new repository:

```bash
ll-builder repo add myrepo https://repo.example.com
```

Add repository with alias:

```bash
ll-builder repo add myrepo https://repo.example.com --alias myalias
```

Remove repository:

```bash
ll-builder repo remove myalias
```

Update repository URL:

```bash
ll-builder repo update myalias https://new-repo.example.com
```

Set default repository:

```bash
ll-builder repo set-default myalias
```

Enable repository mirror:

```bash
ll-builder repo enable-mirror myalias
```

Disable repository mirror:

```bash
ll-builder repo disable-mirror myalias
```

## SEE ALSO

**[ll-builder(1)](./ll-builder.md)**

## HISTORY

Developed in 2023 by UnionTech Software Technology Co., Ltd.
