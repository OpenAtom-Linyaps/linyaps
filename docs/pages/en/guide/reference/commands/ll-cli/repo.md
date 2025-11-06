% ll-cli-repo 1

## NAME

ll\-cli\-repo - Display or modify current repository information

## SYNOPSIS

**ll-cli repo** _subcommand_ [*options*]

## DESCRIPTION

The `ll-cli repo` command is used to manage remote repositories. This command provides a series of subcommands to add, modify, delete, display, and configure repository information.

## SUBCOMMANDS

### add

Add a new repository.

**Usage**: `ll-cli repo add [OPTIONS] NAME URL`

**Parameters**:

- **NAME** _TEXT_ _REQUIRED_: Specify repository name
- **URL** _TEXT_ _REQUIRED_: Repository URL address
- **--alias** _ALIAS_: Repository name alias

**Examples**:

```bash
ll-cli repo add myrepo https://repo.example.com
```

### remove

Remove a repository.

**Usage**: `ll-cli repo remove [OPTIONS] NAME`

**Parameters**:

- **ALIAS** _TEXT_ _REQUIRED_: Repository name alias

**Examples**:

```bash
ll-cli repo remove myrepo
```

### update

Update repository URL.

**Usage**: `ll-cli repo update [OPTIONS] NAME URL`

**Parameters**:

- **ALIAS** _TEXT_ _REQUIRED_: Repository name alias
- **URL** _TEXT_ _REQUIRED_: New repository URL address

**Examples**:

```bash
ll-cli repo update myrepo https://updated-repo.example.com
```

### set-default

Set default repository name.

**Usage**: `ll-cli repo set-default [OPTIONS] NAME`

**Parameters**:

- **Alias** _TEXT_ _REQUIRED_: Repository name alias

**Examples**:

```bash
ll-cli repo set-default myrepo
```

### show

Display repository information.

**Usage**: `ll-cli repo show [OPTIONS]`

**Examples**:

```bash
ll-cli repo show
```

### set-priority

Set repository priority.

**Usage**: `ll-cli repo set-priority ALIAS PRIORITY`

**Parameters**:

- **ALIAS** _TEXT_ _REQUIRED_: Repository name alias
- **PRIORITY** _TEXT_ _REQUIRED_: Repository priority

**Examples**:

```bash
ll-cli repo set-priority myrepo 100
```

### enable-mirror

Enable mirror for repository.

**Usage**: `ll-cli repo enable-mirror [OPTIONS] ALIAS`

**Parameters**:

- **ALIAS** _TEXT_ _REQUIRED_: Repository name alias

**Examples**:

```bash
ll-cli repo enable-mirror myrepo
```

### disable-mirror

Disable mirror for repository.

**Usage**: `ll-cli repo disable-mirror [OPTIONS] ALIAS`

**Parameters**:

- **ALIAS** _TEXT_ _REQUIRED_: Repository name alias

**Examples**:

```bash
ll-cli repo disable-mirror myrepo
```

## OPTIONS

**-h, --help**
: Print help information and exit

**--help-all**
: Expand all help

## EXAMPLES

Display all repository information:

```bash
ll-cli repo show
```

Add a new repository:

```bash
ll-cli repo add deepin https://repo.deepin.com
```

Update repository URL:

```bash
ll-cli repo update deepin https://new-repo.deepin.com
```

Set repository priority:

```bash
ll-cli repo set-priority deepin 50
```

Remove a repository:

```bash
ll-cli repo remove deepin
```

## SEE ALSO

**[ll-cli(1)](./ll-cli.md)**, **[ll-cli-search(1)](./search.md)**, **[ll-cli-install(1)](./install.md)**

## HISTORY

Developed in 2023 by UnionTech Software Technology Co., Ltd.
