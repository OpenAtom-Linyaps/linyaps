# ll-cli alias

Create command aliases for exported binaries of an application.

## Synopsis

```bash
ll-cli alias <alias-name> --from=<appid> --command=<command> [options]
```

## Description

The `alias` subcommand creates an executable wrapper script in `/var/lib/linglong/entries/bin` 
that allows you to run a specific binary from a Linglong application directly from the host shell.

The script content is `exec ll-cli run <appid> -- <command> "$@"`.

Before creating the alias, the package manager checks that the `command` is listed in the 
application's `exportedBinaries` field in its `info.json`. Only commands that are explicitly 
declared can be exported. The script is created atomically using `O_EXCL` to prevent race 
conditions (TOCTOU).

## Options

- `--from=<appid>`: The application ID whose binary you want to alias. **Required.**
- `--command=<command>`: The command name (from the app's `exportedBinaries`) to alias. **Required.**

## Examples

Create an alias named `ls` that runs the `ls` command from application `com.example.app`:

```bash
ll-cli alias ls --from=com.example.app --command=ls
```

After creation, running `ls` in the shell will execute `ll-cli run com.example.app -- ls "$@"`.

## See Also

- [ll-cli run](./run.md)
