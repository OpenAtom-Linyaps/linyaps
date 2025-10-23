% ll-cli-ps 1

## NAME

ll\-cli\-ps - List running applications

## SYNOPSIS

**ll-cli ps** [*options*]

## DESCRIPTION

The `ll-cli ps` command can view running Linyaps applications. This command displays all currently running Linyaps applications in the system and their related information.

## OPTIONS

**-h, --help**
: Print help information and exit

**--help-all**
: Expand all help information

## EXAMPLES

To view running applications, run the `ll-cli ps` command:

```bash
ll-cli ps
```

The `ll-cli ps` command output is as follows:

```text
Application       ContainerID     ProcessID
org.dde.calendar  4e3407a8a052  1279610
```

## SEE ALSO

**[ll-cli(1)](./ll-cli.md)**, **[ll-cli-run(1)](run.md)**, **[ll-cli-kill(1)](kill.md)**

## HISTORY

Developed in 2023 by UnionTech Software Technology Co., Ltd.
