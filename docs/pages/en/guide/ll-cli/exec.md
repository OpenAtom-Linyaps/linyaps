<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# Attach To Container

Use `ll-cli exec` to enter the inside of the running Linglong container.

View the help information for the `ll-cli exec` command:

```bash
ll-cli exec --help
```

Here is the output:

```text
Usage: ll-cli [options] exec 9c41c0af2bad4617aea8485f5aaeb93a "bash"

Options:
  -h, --help        Displays help on commandline options.
  --help-all        Displays help including Qt specific options.
  -e, --env <env>   extra environment variables splited by comma
  -d, --path <pwd>  location to exec the new command

Arguments:
  exec              exec command in container
  containerId       container id
  cmd               command
```

Open a new terminal window and run these commands:

```bash
killall ll-service
ll-service
```

Example of using `ll-cli exec` to get inside a running container:

```bash
ll-cli exec 9c41c0af2bad4617aea8485f5aaeb93a /bin/bash
```

Return to the `ll-service` terminal interface and find that it has entered the application container.

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
