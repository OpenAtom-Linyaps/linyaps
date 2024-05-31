<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# Run FAQ

1. When the application runs to read the application installation resource file under `/usr/share`, why does the reading fail?

   Linglong applications run in a container, and the application data will be mounted to `/opt/apps/<appid>`/. Only system data will exist in the `/usr/share` directory, and there will be no application-related data. Therefore, reading directly from `/usr/share` will fail. Suggested processing: Use the `XDG_DATA_DIRS` environment variable to read resources, and `/opt/apps/<appid>/files/share` will exist in this environment variable search path.
2. The font library file cannot be found when the application is running. Why can the corresponding font library be read when the `deb` package is installed?

   When the `deb` package is installed, it will depend on the corresponding font library file. The Linglong package format adopts a self-sufficient packaging format. Except for the basic system library, `Qt` library and `DTK` library files provided in `runtime`, do not need to be provided by yourself, other dependent data files need to be provided by yourself. It is recommended to put the corresponding data file under `files/share`, and use the environment variable `XDG_DATA_DIRS` to read the path.
3. What is in the Linglong application `runtime`? Can you add some library files to it?

   At present, the `runtime` that Linglong application depends on provides the `Qt` library and the `DTK` library. Because `runtime` has a strict size limit, adding additional library files to `runtime` is currently not allowed.
4. The application runs in the container. Can a configuration file be created in any path of the container during the running process?

   You can create configuration files under `XDG_CONFIG_HOME`.
5. Where is app data saved? Where can I find it outside the container?

   Because Linglong applications follow the principle of non-interference, the `XDG_DATA_HOME`, `XDG_CONFIG_HOME`, `XDG_CACHE_HOME` environment variables are defined in the corresponding path of the host machine, `~/.linglong/<appid>`/. So the user application data will be saved under this path. When writing data while the application is running, it should also be able to read the corresponding environment variable to write the data. Prohibit reading and writing configurations of other applications.
6. The application provides the `dbus service` file, where do I place it? What does the `Exec` segment write?

   When the application provides the `dbus service` file, it needs to be placed in the `entries/dbus-1/services` directory. If `Exec` executes the binary in the Linglong package, use the `--exec` option parameter to execute the corresponding binary.
7. After the app is installed, the launcher cannot find it?

   TryExec=xxx, if xxx does not exist in the $PATH, the application is considered non-existent and will not be displayed.
8. Why is the Icon displayed as a small black dot?

   The desktop file has an 'Icon' field written, but the Icon field name is incorrect or an absolute path is being used."。
9. Why is the Icon field as a gear?

   The desktop file does not provide Icon field.
10. Where are the icons stored?

    svg  -> $PREFIX/share/icons/hicolor/scalable/apps/

    Other formats are stored according to resolution, such as 16x16.

    png/xpm -> $PREFIX/share/icons/hicolor/16X16/apps/
11. Why do `xdg-open` and `xdg-email` that come with the application fail?

    Linglong specially handles `xdg-open` and `xdg-email` in `runtime`, so the application is forbidden to execute the executable file or script of `xdg-open` and `xdg-email` that it carries.
12. Why doesn't the system environment variable used by the application take effect?

    When using environment variables, you need to confirm whether there are corresponding environment variables in the container. If not, you need to contact the Linglong team for processing.
13. The library files required for the application to run were not found. How can I provide them?

    The resource files that the application needs to use and the library files both need to be provided by the application itself. The library files are placed in the `$PREFIX/lib` path.
14. Why has the `Qt WebEngine` rendering process crashed when the application is running?

    Due to the system upgrade of `glibc`, the application fails to use the built-in browser, and the application needs to be re-adapted. A temporary solution is to set the environment variable: `export QTWEBENGINE_DISABLE_SANDBOX=1`.
15. When the application is running, the `libqxcb.so` library cannot be found or the `qtwebengine` error is reported. What can I do?

    When a `qt.conf` file exists, you need to configure the correct path in the file or use the `QTWEBENGINEPROCESS_PATH`, `QTWEBENGINE_RESOURCE_PATH`, `QT_QPA_PLATFORM_PLUGIN_PATH`, and `QT_PLUGIN_PATH` environment variables to configure the search path.
16. Can the application carry the database file by itself and write data to the database during operation?

    The file system in the container is a read-only file system and does not allow data to be written to application resource files.
17. Why does the execution of binary with `suid` and `guid` permissions fail?

    In order to ensure system security, Linglong container prohibits the execution of such permission binaries in the container.
18. Can the input method of uab offline package format not be used under Debian and Ubuntu?

    It is recommended to install the `fictx` input method to experience it.
19. How can I know which packages are installed in a container environment?

    Enter the container environment using the command `ll-builder run --exec bash`. To view the pre-installed packages, utilize the command `cat /var/lib/dpkg/status | grep "^Package: "`. Additionally, for libraries compiled from source code, you can inspect them using `cat /runtime/packages.list`.
