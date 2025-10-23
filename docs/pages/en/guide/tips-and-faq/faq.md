<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# Common Runtime Issues

1. Why does the application fail to read resource files under `/usr/share` during runtime?

   Linyaps applications run in a container environment where application data is mounted under `/opt/apps/<appid>/`. The `/usr/share` directory only contains system data, not application-related data. Therefore, directly reading from `/usr/share` will fail. Recommended solution: Use the `XDG_DATA_DIRS` environment variable to read resources, as `/opt/apps/<appid>/files/share` will be included in this environment variable's search path.

2. Why can't the application find font library files at runtime? Why could it read the corresponding font libraries when installed as a `deb` package?

   When installing a `deb` package, the corresponding font library files are brought in as dependencies. However, the Linyaps package format uses a self-sufficient packaging approach. Except for basic system libraries and the `Qt` and `DTK` libraries provided in the `runtime`, all other dependency data files must be provided by the application itself. It is recommended to place the corresponding data files under `files/share` and use the `XDG_DATA_DIRS` environment variable to read the path.

3. What's in the Linyaps application `runtime`? Can I add some library files to it?

   Currently, the `runtime` that Linyaps applications depend on provides `Qt` libraries and `DTK` libraries. Due to strict size limitations on the `runtime`, adding additional library files to the `runtime` is currently not allowed.

4. Can the application create configuration files in any path within the container during runtime?

   Configuration files can be created under `XDG_CONFIG_HOME`.

5. Where is application data saved? Where can it be found outside the container?

   As Linyaps applications follow the principle of non-interference, the `XDG_DATA_HOME`, `XDG_CONFIG_HOME`, and `XDG_CACHE_HOME` environment variables are defined to correspond to paths under the host's `~/.linglong/<appid>/`. Therefore, user application data will be saved in this path, and applications should read the corresponding environment variables when writing data during runtime. Reading and writing configurations of other applications is prohibited.

6. The application provides a `dbus service` file, how should it be placed? What should be written in the `Exec` field?

   When the application provides a `dbus service` file, it needs to be placed in the `entries/dbus-1/services` directory. If `Exec` executes a binary within the Linyaps package, use the `--exec` option parameter to execute the corresponding binary.

7. The launcher cannot find the application after installation?

   TryExec=xxx, when xxx does not exist in the $PATH, the application will be considered non-existent and will not be displayed.

8. Why is the icon displayed as a small black dot?

   The desktop file has an Icon field, but the Icon field name is incorrect or an absolute path is used.

9. Why is the icon displayed as a gear?

   The desktop file does not provide an Icon field.

10. Where should icons be stored?

    svg → $PREFIX/share/icons/hicolor/scalable/apps/

    Other formats are stored by resolution, such as 16x16

    png/xpm → $PREFIX/share/icons/hicolor/16X16/apps/

11. Why do the application's built-in `xdg-open` and `xdg-email` fail?

    Linyaps specially handles `xdg-open` and `xdg-email` in the `runtime`, so applications are prohibited from executing their own xdg-open and xdg-email executables or scripts.

12. Why doesn't the system environment variable used by the application take effect?

    When using environment variables, you need to confirm whether the corresponding environment variable exists in the container. If not, you need to contact the Linyaps team for handling.

13. The library files needed for application runtime are not found, how to provide them?

    Resource files and library files that the application needs to use must be provided by the application itself. Library files should be placed under the `$PREFIX/lib` path.

14. Why does the `Qt WebEngine` rendering process crash during application runtime?

    Due to system upgrades of `glibc`, applications fail when using the built-in browser and need to be re-adapted. A temporary solution is to set the environment variable: `export QTWEBENGINE_DISABLE_SANDBOX=1`.

15. The application cannot find the `libqxcb.so` library or reports qtwebengine errors at runtime?

    When a `qt.conf` file exists, configure the correct path in the file, or use environment variables `QTWEBENGINEPROCESS_PATH`, `QTWEBENGINE_RESOURCES_PATH`, `QT_QPA_PLATFORM_PLUGIN_PATH`, `QT_PLUGIN_PATH` to configure the search path.

16. Can the application carry its own database files and write data to the database during runtime?

    The file system inside the container is a read-only file system, and writing data to application resource files is not allowed.

17. Why does executing binaries with `suid`, `guid` permissions fail?

    To ensure system security, Linyaps containers prohibit the execution of such privileged binaries.

18. UAB offline package format cannot use input method under Debian and Ubuntu?

    It is recommended to install the `fictx` input method for better experience.

19. How to know which packages are installed in the container environment?

    Use `ll-builder run --exec bash` to enter the container environment, and use the command `cat /var/lib/dpkg/status | grep "^Package: "` to view pre-installed packages. Additionally, libraries compiled from source can be viewed using `cat /runtime/packages.list`.

20. Why doesn't the application tray display after the application starts?

    This may be because the application registered the tray using the same service name. According to the KDE/freedesktop StatusNotifierItem specification, applications should register service names as org.kde.StatusNotifierItem-`<process id>`-`<instance number>`. In Linyaps applications, the pid during application runtime is 19. You can check whether a service has been registered using the following command: `dbus-send --session --print-reply --dest=org.freedesktop.DBus /org/freedesktop/DBus org.freedesktop.DBus.NameHasOwner string:org.kde.StatusNotifierItem-19-1`. If `boolean true` exists, it means the service has been registered.
