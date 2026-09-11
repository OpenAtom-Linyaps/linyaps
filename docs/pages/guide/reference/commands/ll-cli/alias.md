# ll-cli alias

Create command aliases for exported binaries of an application.

## Synopsis

```bash
ll-cli alias <appid> [--name=<name>] [-- <command args...>] [-f|--force]
```

## Description

The `alias` subcommand creates an executable wrapper script under
`/var/lib/linglong/entries/bin/apps/<appid>/bin/` and a symlink in
`/var/lib/linglong/entries/bin/` pointing to it. This allows you to run a
specific binary from a Linglong application directly from the host shell.

When no command arguments are given (no `--`), the script content is
`exec ll-cli run <appid> -- '<cmd[0]>' '<cmd[1]>' ... "$@"`, where `cmd` is
the application's `command` field from `info.json`.

When command arguments are given after `--`, they form a custom command
string. Each argument is individually shell-quoted and joined with spaces,
then embedded in the script as `exec ll-cli run <appid> -- <quoted-args> "$@"`.

By default the script is created atomically using `O_EXCL` to prevent race
conditions (TOCTOU). Use `--force` to overwrite an existing script.

## Options

- `<appid>`: The application ID to create an alias for. **Required positional argument.**
- `--name=<name>`: The binary name (script filename) to create. **Optional.** If omitted, defaults to `<appid>` when no command arguments are given, or `bin` when command arguments are provided.
- `-f, --force`: Overwrite an existing script with the same name.
- `-- <command args...>`: Extra arguments after `--` form a custom command. Each argument is shell-quoted and joined into a single command string.

## Examples

Create an alias with the default name (appid) using the app's full command array:

```bash
ll-cli alias com.example.app
```

Create an alias with a custom name using the app's full command array:

```bash
ll-cli alias com.example.app --name=mytool
```

Create an alias named `bin` with a custom command (args after `--`):

```bash
ll-cli alias com.example.app -- ls --color=auto
```

Create an alias with a custom name and custom command:

```bash
ll-cli alias com.example.app --name=ls -- ls --color=auto
```

Overwrite an existing alias:

```bash
ll-cli alias com.example.app --name=mytool --force
```

## See Also

- [ll-cli run](./run.md)
