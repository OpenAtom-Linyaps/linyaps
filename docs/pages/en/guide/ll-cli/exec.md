<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# Attach To Container

Use `ll-cli exec` to enter the inside of the running linyaps container.

View the help information for the `ll-cli exec` command:

```bash
ll-cli exec --help
```

Here is the output:

```text
Execute commands in the currently running sandbox
Usage: ll-cli [OPTIONS] [SUBCOMMAND]

Positionals:
  INSTANCE TEXT REQUIRED      Specify the application running instance(you can get it by ps command)
  COMMAND TEXT ...            Run commands in a running sandbox

Options:
  -h,--help                   Print this help message and exit
  --help-all                  Expand all help
  --working-directory PATH:DIR
                              Specify working directory

If you found any problems during use,
You can report bugs to the linyaps team under this project: https://github.com/OpenAtom-Linyaps/linyaps/issues
```

Open a new terminal window and run these commands:

Example of using `ll-cli exec` to get inside org.dde.calendar container:

1. using `ll-cli ps` to get container id:

```bash
App                                       ContainerID      Pid
main:org.dde.calendar/5.14.5.0/x86_64     c3b5ce363172     539537
```

2. enter the org.dde.calendar container
```bash
ll-cli exec main:org.dde.calendar/5.14.5.0/x86_64 /bin/bash
```

Use `ls -l /` to view the root directory structure. The output is as follows:

```text
lrwxrwxrwx   1 root   root        8  July 27 20:58 bin -> /usr/bin
drwxr-xr-x   7 root   root      420  July 27 20:58 dev
drwxr-xr-x 119 nobody nogroup 12288  July 27 20:37 etc
drwxr-xr-x   3 root   root       60  July 27 20:58 home
lrwxrwxrwx   1 root   root        8  July 27 20:58 lib -> /usr/lib
lrwxrwxrwx   1 root   root       10  July 27 20:58 lib32 -> /usr/lib32
lrwxrwxrwx   1 root   root       10  July 27 20:58 lib64 -> /usr/lib64
lrwxrwxrwx   1 root   root       11  July 27 20:58 libx32 -> /usr/libx32
drwxr-xr-x   2 root   root       40  July 27 20:58 ll-host
drwxr-xr-x   3 root   root       60  July 27 20:58 opt
dr-xr-xr-x 365 nobody nogroup     0  July 27 20:58 proc
drwxr-xr-x   6 root   root      120  July 27 20:58 run
drwxr-xr-x   7 nobody nogroup  4096  January  1  1970 runtime
dr-xr-xr-x  13 nobody nogroup     0  July 21 16:43 sys
drwxr-xr-x   3 root   root     4096  July 27 20:58 tmp
drwxr-xr-x  15 nobody nogroup  4096  June 30 16:22 usr
drwxr-xr-x  13 nobody nogroup  4096  June 30 16:23 var
```
