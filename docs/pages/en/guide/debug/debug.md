<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# Debugging Linyaps Applications

The following tutorial uses the org.deepin.demo project mentioned in the "Building Tools" section as an example.

We place the project in `/tmp`. When following the tutorial, **pay attention to replace the paths**.

## Debugging with gdb in Terminal

### Running Application in Debug Environment

In the [Running Compiled Applications](../reference/commands/ll-builder/run.md) section, we already know that using `ll-builder run --exec /bin/bash` can run the compiled application and enter the container terminal. Simply add the `--debug` parameter after `run` to run the container in debug environment and enter it. The main differences between debug environment and non-debug environment are as follows:

1. Debug environment uses binary+develop modules of base and runtime, while non-debug environment uses binary modules. The gdb tool is in the develop module of base.
2. Debug environment uses binary+develop modules of app, while non-debug environment uses binary modules by default (debug symbols are usually saved to develop module).
3. Debug environment generates linglong/gdbinit file in project directory and mounts it to ~/.gdbinit path in container.

Please execute `ll-builder run --debug --exec /bin/bash` in project directory to enter debug environment container, then execute `gdb /opt/apps/org.deepin.demo/binary/demo` to start application debugging, which is no different from using command line debugging externally. This is thanks to the debugging linglong/gdbinit file providing initial configuration for `gdb`.

### Debugging Application in Runtime Environment

There are minor differences between debug environment and normal user runtime environment. If you need to debug application directly in runtime environment, you can use `ll-builder run --exec /bin/bash` to enter container, then execute `gdbserver 127.0.0.1:12345 /opt/apps/org.deepin.demo/bin/demo`. gdbserver will use tcp protocol to listen on port 12345 and wait for gdb connection.

Open another host terminal, execute `gdb` in project directory, and input the following commands line by line:

```txt
set substitute-path /project /tmp/org.deepin.demo
set debug-file-directory /tmp/org.deepin.demo/linglong/output/develop/files/lib/debug
target remote 127.0.0.1:12345
```

Next, refer to the tutorial in "Run compiled App", run `bash` in the container through the `ll-builder run` command, and run the application to be debugged through `gdbserver`:

_If runtime environment doesn't have gdbserver command, please check if application uses org.deepin.base as base and try upgrading to latest version of org.deepin.base._

## Debugging with gdb in vscode

First install C/C++ extension for vscode. Since vscode runs on host machine, it also needs to provide debugging for Linyaps container application through gdbserver. Similar to previous step, start gdbserver in runtime environment first, then configure launch.json file in vscode. Configuration is as follows:

```json
{
  "version": "0.2.0",
  "configurations": [
    {
      "name": "(gdb) linglong",
      "type": "cppdbg",
      "request": "launch",
      "program": "${workspaceFolder}/linglong/output/binary/files/bin/demo",
      "args": [],
      "stopAtEntry": true,
      "cwd": "${workspaceFolder}",
      "MIMode": "gdb",
      "miDebuggerServerAddress": "127.0.0.1:12345",
      "setupCommands": [
        {
          "text": "set substitute-path /project ${workspaceFolder}"
        },
        {
          "text": "set debug-file-directory ${workspaceFolder}/linglong/output/develop/files/lib/debug"
        }
      ]
    }
  ]
}
```

Some configurations need to be changed according to actual project:

- "program": "${workspaceFolder}/linglong/output/binary/files/bin/demo",

  This is the binary file passed to gdb, `demo` needs to be changed to actual binary filename of project

- "stopAtEntry": true

  This requires gdb to automatically stop at main function, can be set to false if not needed

- "miDebuggerServerAddress": "127.0.0.1:12345"

  This is the remote address for gdb connection, if port is not default 12345 when starting gdbserver, need to modify to actual port.

- "text": "set substitute-path /project ${workspaceFolder}"

  This sets source path substitution, `${workspaceFolder}` will be automatically replaced by vscode with current working directory, can be modified to actual path if needed.

- "text": "set debug-file-directory ${workspaceFolder}/linglong/output/develop/files/lib/debug"

  This sets debug files directory, if debug symbols are not saved to `develop` module, need to modify to actual location.

## Debugging with gdb in Qt Creator

Qt Creator also integrates gdb support. After starting Qt Creator, open menu bar `Debug` -> `Start Debugging` -> `Connect to Debug Server`, and fill in the dialog that pops up:

```text
Server Port: `12345`

Local Executable: `/tmp/org.deepin.demo/linglong/output/binary/files/bin/demo`

Working Directory: `/tmp/org.deepin.demo`

Init Commands: `set substitute-path /project /tmp/org.deepin.demo`

Debug Information: `/tmp/org.deepin.demo/linglong/output/develop/files/lib/debug`
```

Configuration is roughly as shown in figure:

![qt-creator](images/qt-creator.png)

After configuration, `QtCreator` can be used normally for debugging.

## Saving Debug Symbols

Linyaps automatically strips binary debug symbols after building applications and stores them in `$PREFIX/lib/debug` directory. However, some toolchains strip debug symbols during build process in advance, which causes Linyaps unable to find these symbols in binary files. If your project uses qmake, need to add following configuration in pro file:

```bash
# Linyaps sets -g option in CFLAGS and CXXFLAGS environment variables, qmake needs to inherit this environment variable
QMAKE_CFLAGS += $$(CFLAGS)
QMAKE_CXXFLAGS += $$(CXXFLAGS)
# Use debug option to avoid qmake automatically stripping debug symbols
CONFIG += debug
```

cmake automatically uses cflags and cxxflags environment variables, so no additional configuration is needed. Other build tools can refer to their documentation.

## Downloading Debug Symbols from Debian Repository

Since base images don't contain debug symbols, if you need to debug system dependency libraries of applications, you need to manually download debug symbol packages from corresponding Debian repository of base. Specific steps are as follows:

1. Enter container command line environment using one of the following commands:

   ```bash
   ll-builder run --bash
   # or
   ll-cli run $appid --bash
   ```

2. Check repository address used by base image:

   ```bash
   cat /etc/apt/sources.list
   ```

3. Open repository address in host browser and locate directory where dependency library deb packages are located:
   - Use command `apt-cache show <package-name> | grep Filename` to check deb package path in repository
   - Complete download address is: repository address + deb package path

   For example, to download debug symbols package for libgtk-3-0:

   ```bash
   apt-cache show libgtk-3-0 | grep Filename
   # Output: pool/main/g/gtk+3.0/libgtk-3-0_3.24.41-1deepin3_amd64.deb
   # Complete directory: <repo-url>/pool/main/g/gtk+3.0/
   ```

4. Look for corresponding debug symbol packages in that directory, usually has two naming formats:
   - `<package-name>-dbgsym.deb`
   - `<package-name>-dbg.deb`

5. Download and extract debug symbol package:

   ```bash
   dpkg-deb -R <package-name>-dbgsym.deb /tmp/<package-name>
   ```

6. Configure debugger to find debug symbols:
   When setting debug-file-directory in scenarios above, append extracted directory, separated by colon:
   ```
   ${workspaceFolder}/linglong/output/develop/files/lib/debug:/tmp/<package-name>/usr/lib/debug
   ```

This way debugger can find debug symbols of system dependency libraries in extracted directory.
