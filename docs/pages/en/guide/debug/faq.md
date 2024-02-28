<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# Run FAQ

1. When the application runs to read the application installation resource file under `/usr/share`, why does the reading fail?

   Linglong applications run in a container, and the application data will be mounted to `/opt/apps/<appid>`/. Only system data will exist in the `/usr/share` directory, and there will be no application-related data. Therefore, reading directly from `/usr/share` will fail. Suggested processing: Use the `XDG_DATA_DIRS` environment variable to read resources, and `/opt/apps/<appid>/files/share` will exist in this environment variable search path.

2. The font library file cannot be found when the application is running. Why can the corresponding font library be read when the `deb` package is installed?

   When the `deb` package is installed, it will depend on the corresponding font library file. The Linglong package format adopts a self-sufficient packaging format. Except for the basic system library, `qt` library and `dtk` library files provided in `runtime`, do not need to be provided by yourself, other dependent data files need to be provided by yourself. It is recommended to put the corresponding data file under `files/share`, and use the environment variable `XDG_DATA_DIRS` to read the path.

3. What is in the Linglong application `runtime`? Can you add some library files to it?

   At present, the `runtime` that Linglong application depends on provides the `qt` library and the `dtk` library. Because `runtime` has a strict size limit, adding additional library files to `runtime` is currently not allowed.

4. The application runs in the container. Can a configuration file be created in any path of the container during the running process?

   This is not allowed. The file system in the container is a read-only file system and configuration files are not allowed to be created under arbitrary paths.

5. Where is app data saved? Where can I find it outside the container?

   Because Linglong applications follow the principle of non-interference, the `XDG_DATA_HOME`, `XDG_CONFIG_HOME`, `XDG_CACHE_HOME` environment variables are defined in the corresponding path of the host machine, `~/.linglong/<appid>`/. So the user application data will be saved under this path. When writing data while the application is running, it should also be able to read the corresponding environment variable to write the data. Mutual configuration calls between applications are prohibited.

6. The application provides the `dbus service` file, where do I place it? What does the `Exec` segment write?

   When the application provides the `dbus service` file, it needs to be placed in the `entries/dbus-1/services` directory. If `Exec` executes the binary in the Linglong package, use the `--exec` option parameter to execute the corresponding binary.

7. Can the application download files to the `$HOME` directory by default? I downloaded the file, why can't I find it on the host machine?

   Linglong specification does not allow files and directories to be created in the `$HOME` directory.

8. Why does the desktop shortcut icon appear as a gear or blank? How are the icon files placed?

    If the gear shaped icon is displayed, the icon has not been obtained. You need to confirm whether the `Icon` path name is correct. When the icon is empty, the `tryExec` field exists. When the command does not exist, it will cause the shortcut to display improperly. Place the application icon in the `icons` directory. The directory structure should be consistent with the system `icons` directory structure. The recommended path is `icons/hicolor/scalable/apps/org.desktopspec.demo.svg`. Use `svg` format icons. Refer to the icon file format specification. If you use a non-vector format, please place the icon according to the resolution. Note that the `desktop` file should not write the icon path, but directly write the icon name.

9. Why do `xdg-open` and `xdg-email` that come with the application fail?

    Linglong specially handles `xdg-open` and `xdg-email` in `runtime`, so the application is forbidden to execute the executable file or script of `xdg-open` and `xdg-email` that it carries.

10. Why doesn't the system environment variable used by the application take effect?

    When using environment variables, you need to confirm whether there are corresponding environment variables in the container. If not, you need to contact the Linglong team for processing.

11. The library files required for the application to run were not found. How can I provide them?

    The resource files that the application needs to use and the library files both need to be provided by the application itself. The library files are placed in the `files/lib` path.

12. Where can I choose the application download directory?

    Currently Linglong users can only choose the `Desktop`, `Documents`, `Downloads`, `Music`, `Pictures`, `Videos`, `Public`, and `Templates` directories under the user's home directory, and cannot be downloaded to other directories.

13. Why has the `QTWebEngine` rendering process crashed when the application is running?

    Due to the system upgrade of `glibc`, the application fails to use the built-in browser, and the application needs to be re-adapted. A temporary solution is to set the environment variable: `export QTWEBENGINE_DISABLE_container=1`.

14. When the application is running, the `libqxcb.so` library cannot be found or the `qtwebengine` error is reported. What can I do?

    When a `qt.conf` file exists, you need to configure the correct path in the file or use the `QTWEBENGINEPROCESS_PATH`, `QTWEBENGINERESOURCE_PATH`, `QT_QPA_PLATFORM_PLUGIN_PATH`, and `QT_PLUGIN_PATH` environment variables to configure the search path.

15. The application running error message `gpu_data_manager_impl_private`, how can I solve it?

    The current temporary solution is to add `--no-container`.

    Reference: <https://github.com/Automattic/simplenote-electron/issues/3044>

16. Can the application carry the database file by itself and write data to the database during operation?

    The file system in the container is a read-only file system and does not allow data to be written to application resource files.

17. Why does the execution of binary with `suid` and `guid` permissions fail?

    In order to ensure system security, Linglong container prohibits the execution of such permission binaries in the container.

18. Can the input method of uab offline package format not be used under Debian and Ubuntu?

    It is recommended to install the `fictx` input method to experience it.
