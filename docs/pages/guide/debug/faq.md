<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# 常见运行问题

1. 应用运行读取 `/usr/share`下应用安装资源文件，为什么读取失败？

   玲珑应用是在容器环境中运行，应用数据会挂载到 `/opt/apps/<appid>`/下，`/usr/share`目录下只会存在系统数据，不会存在应用相关数据。因此直接读取 `/usr/share`下会失败。建议处理：采用 `XDG_DATA_DIRS`环境变量读取资源，`/opt/apps/<appid>/files/share`会存在在此环境变量搜索路径中。
2. 应用运行时找不到字体库文件？为什么 `deb`包安装时能读取到对应的字体库？

   `deb`包安装时，会依赖带入对应的字体库文件。而玲珑包格式采用自给自足打包格式。除了基本的系统库，`runtime`里面提供的 `Qt`库与 `dtk`库文件不用自己提供外，其他依赖数据文件，均需自己提供。建议对应的数据文件放入 `files/share`下，采用环境变量 `XDG_DATA_DIRS`读取路径。
3. 玲珑应用 `runtime`里面有什么？能不能往里面添加一些库文件进去？

   目前玲珑应用依赖的 `runtime`里面提供的是 `Qt`库与 `dtk`库。因 `runtime`有严格的大小限制。目前不允许往 `runtime`里面添加额外的库文件。
4. 应用在容器内运行，运行过程中能不能往容器任意路径下创建配置文件？

   可以在 `XDG_CONFIG_HOME` 下创建配置文件。
5. 应用数据保存到哪里？在容器外哪里能找到？

   因玲珑应用遵循互不干涉原则，`XDG_DATA_HOME`、`XDG_CONFIG_HOME`、`XDG_CACHE_HOME`环境变量被定义到宿主机 `~/.linglong/<appid>`/对应的路径下，因此用户应用数据会保存在此路径下，应用运行过程中写入数据时，也应该读取对应的环境变量写入数据。禁止应用间互相配置调用。
6. 应用提供了 `dbus service`文件，如何放置？`Exec`段写什么？

   应用提供 `dbus service`文件时，需要放到 `entries/dbus-1/services`目录下，如果 `Exec`执行玲珑包内二进制，使用 `--exec`选项参数执行对应的二进制。
7. 应用安装后，启动器无法找到？

   TryExec=xxx, 当xxx 在 $PATH 路径中不存在时，会认为该应用不存在不予显示。
8. 为什么图标显示为小黑点？

   desktop 写了icon字段，且icon字段名称不对或者使用绝对路径。
9. 为什么图标显示为齿轮？

   desktop 未提供icon信息。
10. 图标存放在哪个路径？

    svg  -> $PREFIX/share/icons/hicolor/scalable/apps/

    其他格式按分辨率存放，如16X16

    png/xpm -> $PREFIX/share/icons/hicolor/16X16/apps/
11. 应用自带的 `xdg-open`、`xdg-email`为什么失效？

    `runtime`中玲珑特殊处理了 `xdg-open`、`xdg-email`，因此应用禁止执行自己携带的xdg-open、xdg-email可执行文件或者脚本。
12. 应用使用系统环境变量未生效，为什么？

    当使用环境变量时，需要确认容器内是否存在对应的环境变量，如果没有，需要联系玲珑团队处理。
13. 应用运行需要的库文件没找到，如何提供？

    应用需要使用的资源文件，与库文件需要应用自身提供。库文件放到 `$PREFIX/lib` 路径下。
14. 应用运行时，为什么 `Qt WebEngine`渲染进程已崩溃？

    因系统升级了 `glibc`，导致应用使用内置浏览器时失败，需要应用重新适配。临时解决方案是设置环境变量：`export QTWEBENGINE_DISABLE_SANDBOX=1`。
15. 应用运行时，找不到 `libqxcb.so`库或者 `qtwebengin` 报错？

    存在 `qt.conf`文件时，在文件中配置正确路径，或者使用 `QTWEBENGINEPROCESS_PATH`、`QTWEBENGINERESOURCE_PATH`、`QT_QPA_PLATFORM_PLUGIN_PATH`、`QT_PLUGIN_PATH`环境变量配置搜索路径。
16. 应用能否自己携带数据库文件，并在运行中往数据库中写入数据？

    容器内文件系统是只读文件系统，不允许往应用资源文件中写入数据。
17. 为什么执行携带 `suid`、`guid`权限二进制失效？

    玲珑容器为保证系统安全，容器内禁止执行此类权限二进制。
18. uab离线包格式在debian、ubuntu下输入法无法使用？

    建议安装 `fictx`输入法后体验。
19. 怎么知道容器环境中安装了哪些包？

    `ll-builder run --exec bash` 进入容器环境，使用  `cat /var/lib/dpkg/status | grep "^Package: "` 命令查看预装的软件包，另外源码编译的库可以使用 `cat /runtime/packages.list` 查看。
