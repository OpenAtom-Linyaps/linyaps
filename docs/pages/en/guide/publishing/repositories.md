# Repositories

A repository is a remote service used by Linyaps to store and distribute applications and runtime environments. Clients query package metadata from repositories and pull required content during installation. Linyaps configures a default repository during installation, and users can configure multiple official, community, or self-hosted repositories at the same time.

## Repository Settings

View the current configuration with:

```bash
ll-cli repo show
```

The output includes the default repository and the name, URL, alias, and priority of each repository.

| Setting | Description |
| --- | --- |
| Name | Remote repository name: the first argument supplied to `repo add`. |
| URL | Repository service address. Changing it does not change the repository name or alias. |
| Alias | Local identifier for the repository on the current system. If no alias is set, the repository name is used. `remove`, `update`, `set-default`, `set-priority`, and `--repo` identify repositories by alias. |
| Priority | Repository order during multi-repository resolution. Higher values have higher priority; negative values are allowed. See [Multi-repository Behavior](#multi-repository-behavior) for search and installation selection rules. |
| Default | The repository marked as default in the configuration. This setting is deprecated; repository priorities are recommended. Selecting a default also raises its priority above every current repository. |

Aliases must be unique on the local machine. Use short, meaningful aliases such as `stable`, `testing`, or `company`, and use the same alias for subsequent operations.

## Manage Repositories

Use `ll-cli repo` to manage repository configuration. The following examples use `company` as the repository name and `corp` as the local alias.

### Add a Repository

```bash
ll-cli repo add company https://repo.example.com --alias corp
```

If a separate alias is unnecessary, omit `--alias`:

```bash
ll-cli repo add testing https://testing-repo.example.com
```

Adding a repository only registers its remote address locally. It does not create a remote repository or automatically make it the default.

### Update a Repository URL

After a repository service moves, keep its alias and update its URL:

```bash
ll-cli repo update corp https://new-repo.example.com
```

### Adjust Repository Priority

```bash
ll-cli repo set-priority corp 200
ll-cli repo set-priority testing 100
```

Priorities are compared numerically and need not be consecutive. Leave gaps to make later insertion easier.

### Set the Default Repository (Deprecated)

```bash
ll-cli repo set-default corp
```

This also raises the priority of `corp` above every current repository. Subsequent installations without `--repo` therefore search that repository first.

### Remove a Repository

```bash
ll-cli repo remove corp
```

Removal deletes only the local repository configuration, not remote content. At least one repository must remain, so the only repository cannot be removed. If the default is removed, the client selects the highest-priority remaining repository as the new default.

## Multi-repository Behavior

Search and installation behave differently when multiple repositories are configured.

### Search

Without an explicit repository, `ll-cli search` searches every configured repository. The Repository column identifies the package source:

```bash
ll-cli search org.deepin.calculator
```

To search only one repository, use its alias:

```bash
ll-cli search org.deepin.calculator --repo corp
```

### Installation

Without an explicit repository, the client groups repositories by priority from highest to lowest:

1. Query the highest-priority group.
2. If that group has matching packages, select the newest candidate in the group and do not query lower-priority repositories.
3. If it has no match, continue with the next priority group.

Repository priority therefore takes precedence over versions across repositories. If a priority-200 repository contains the target application, an installation without `--repo` selects it even if a priority-100 repository has a newer version.

To bypass priorities and install from a specific repository, use `--repo`:

```bash
ll-cli install org.deepin.calculator --repo testing
```

Repositories with the same priority belong to one priority group. The client queries every repository in that group and selects the newest matching version. For a completely predictable package source, specify `--repo` explicitly instead of relying on automatic selection among equal-priority repositories.

### Recommended Configuration

- When stable and testing repositories coexist, make stable the default with a higher priority and use `--repo testing` for temporary tests.
- Give an enterprise repository higher priority when it must override applications of the same name in public repositories.
- To expand search coverage without changing the default installation source, keep a newly added repository below the default repository's priority.
- After changing the configuration, run `ll-cli repo show` again and confirm the default, aliases, and priorities.

See [`ll-cli repo`](../reference/commands/ll-cli/repo.md) for the command reference.
