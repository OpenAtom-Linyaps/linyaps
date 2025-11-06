% ll-builder-repo 1

## NAME

ll-builder-repo - Display and manage repositories

## SYNOPSIS

**ll-builder repo** [*options*] _subcommand_

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

**add** _name_ _url_ [*options*]
: Add new repository configuration

**remove** _alias_
: Remove repository configuration

**update** _alias_ _url_
: Update repository URL

**set-default** _alias_
: Set default repository name

**enable-mirror** _alias_
: Enable repository mirror

**disable-mirror** _alias_
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
