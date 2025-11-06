% ll-cli-exec 1

## NAME

ll\-cli\-exec - Execute commands in the currently running sandbox

## SYNOPSIS

**ll-cli enter** [*options*] _instance_ _command_...

## DESCRIPTION

The `ll-cli enter` command can enter the interior of a running container. This command allows users to execute specified commands in the running application container.

## OPTIONS

**-h, --help**
: Print help information and exit

**--help-all**
: Expand all help information

**--working-directory** _PATH:DIR_
: Specify the working directory

## POSITIONAL ARGUMENTS

**INSTANCE** _TEXT_ _REQUIRED_
: Specify the running application instance (can be obtained through the ps command)

**COMMAND** _TEXT_...
: Run commands in the running sandbox

## EXAMPLES

Using `ll-cli enter` to enter the interior of a running container:

1. Use `ll-cli ps` to get the running container ID:

```bash
App                                       ContainerID      Pid
main:org.dde.calendar/5.14.5.0/x86_64     c3b5ce363172     539537
```

2. Enter the `org.dde.calendar` container interior:

```bash
ll-cli enter main:org.dde.calendar/5.14.5.0/x86_64 /bin/bash
```

`ls -l /` to view the root directory structure, output as follows:

```text
lrwxrwxrwx   1 root   root        8 Jul 27 20:58 bin -> /usr/bin
drwxr-xr-x   7 root   root      420 Jul 27 20:58 dev
drwxr-xr-x 119 nobody nogroup 12288 Jul 27 20:37 etc
drwxr-xr-x   3 root   root       60 Jul 27 20:58 home
lrwxrwxrwx   1 root   root        8 Jul 27 20:58 lib -> /usr/lib
lrwxrwxrwx   1 root   root       10 Jul 27 20:58 lib32 -> /usr/lib32
lrwxrwxrwx   1 root   root       10 Jul 27 20:58 lib64 -> /usr/lib64
lrwxrwxrwx   1 root   root       11 Jul 27 20:58 libx32 -> /usr/libx32
drwxr-xr-x   2 root   root       40 Jul 27 20:58 ll-host
drwxr-xr-x   3 root   root       60 Jul 27 20:58 opt
dr-xr-xr-x 365 nobody nogroup     0 Jul 27 20:58 proc
drwxr-xr-x   6 root   root      120 Jul 27 20:58 run
drwxr-xr-x   7 nobody nogroup  4096 Jan  1  1970 runtime
dr-xr-xr-x  13 nobody nogroup     0 Jul 21 16:43 sys
drwxr-xr-x   3 root   root     4096 Jul 27 20:58 tmp
drwxr-xr-x  15 nobody nogroup  4096 Jun 30 16:22 usr
drwxr-xr-x  13 nobody nogroup  4096 Jun 30 16:23 var
```

## SEE ALSO

**[ll-cli(1)](./ll-cli.md)**, **[ll-cli-ps(1)](ps.md)**, **[ll-cli-run(1)](run.md)**

## HISTORY

Developed in 2023 by UnionTech Software Technology Co., Ltd.
