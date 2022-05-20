# FAQ

## 1. 应用运行读取/usr/share下应用安装资源文件，为什么读取失败？

玲珑应用是在沙箱环境中运行，应用数据会挂载到/opt/apps/`<appid>`/下，/usr/share目录下只会存在系统数据，不会存在应用相关数据。因此直接读取/usr/share下会失败。建议处理：采用XDG_DATA_DIRS环境变量读取资源，/opt/apps/`<appid>`/files/share会存在在此环境变量搜索路径中。

## 2. 应用安装的数据在系统中会存在哪里？能不能在沙箱中直接修改内容调试应用？

v20系统会安装到/data/linglong/layers目录下，v23系统会安装到/persistent/linglong/layers目录下。沙箱内数据基本是只读挂载，建议修改调试时，在安装目录下修改。

## 3. 应用运行时找不到字体库文件？为什么deb包安装时能读取到对应的字体库？

deb包安装时，会依赖带入对应的字体库文件。而玲珑包格式采用自给自足打包格式。除了基本的系统库，runtime里面提供的qt库与dtk库文件不用自己提供外，其他依赖数据文件，均需自己提供。建议对应的数据文件放入files/share下，采用环境变量XDG_DATA_DIRS读取路径。

## 4. 玲珑应用runtime里面有什么？能不能往里面添加一些库文件进去？

目前玲珑应用依赖的runtime里面提供的是qt库与dtk库。因runtime有严格的大小限制。目前不允许往runtime里面添加额外的库文件。

## 5. 应用在沙箱内运行，运行过程中能不能往沙箱任意路径下创建配置文件？

这是不允许的行为，沙箱内文件系统是只读文件系统，不允许随意路径下创建配置文件。

## 6. 应用数据保存到哪里？在沙箱外哪里能找到？

因玲珑应用遵循互不干涉原则，XDG_DATA_HOME,XDG_CONFIG_HOME,XDG_CACHE_HOME环境变量被定义到宿主机~/.linglong/`<appid>`/对应的路径下，因此用户应用数据会保存在此路径下，应用运行过程中写入数据时，也应该读取对应的环境变量写入数据。禁止应用间互相配置调用。

## 7. 应用走gio接口，打开特定类型文件，无法打开？

走gio接口，打开特定类型文件，会去沙箱内${XDG_DATA_DIRS}/applications/mineinfo.cache文件中，查找对应desktop打开。因玲珑采用沙箱运行，因此需要将打开类型传出沙箱外。玲珑在runtime中伪造了一个desktop文件，添加了各种mimetype类型，而desktop的Exec执行的是沙箱内特制的xdg-open命令，会将打开类型传出沙箱外执行。如果类型无法打开，需要确认desktop文件中是否添加对应了对应的mimetype。如果没有，需要联系玲珑团队添加。如果应用自定义了mimetype类型，需要按照entries/mime/packages/org.desktopspec.demo.xml格式存放。并且此类型需要添加到runtime里面进行存储。

## 8. 应用提供了dbus service文件，如何放置？Exec段写什么？

应用提供dbus service文件时，需要放到entries/dbus-1/services目录下，如果Exec执行玲珑包内二进制，使用--exec选项参数执行对应的二进制。

## 9. 应用能不能默认往$HOME目录下下载文件？下载了文件，为什么宿主机器下找不到？

v23规范，不允许往$HOME目录下创建文件与目录。

## 10. 桌面快捷方式为什么显示齿轮状？或者为空？图标文件如何放置？

显示齿轮状是图标未获取到，需要确认Icon路径名称是否正确。图标为空时，是存在tryExec字段，当命令不存在时，会导致快捷方式显示异常。放置应用图标 icons，目录结构与系统 icons 目录结构保持一致即可，建议路径为 icons/hicolor/scalable/apps/org.desktopspec.demo.svg，使用 svg 格式图标。参考图标文件格式规范如果使用非矢量格式，请按照分辨率来放置图标，注意desktop文件不要写死图标路径，直接写图标名即可。

## 11. 应用自带的xdg-open,xdg-email为什么失效？

runtime中玲珑特殊处理了xdg-open,xdg-email，因此应用禁止带此二进制。

## 12. 为什么应用运行时，调用系统命令失效？

在v23系统中，glibc版本是2.35版本，有的应用携带低版本的c库，会导致系统命令执行失败。因此应用打包时不要携带c库，同时达到减小软件包体积的效果。

## 13. 为什么desktop文件在启动器显示英文？

目前dde会处理自研应用desktop名称翻译，因此命名需要和原desktop保持一致。或者联系dde那边修改对应软件包。

## 14. 应用使用系统环境变量未生效。

当使用环境变量时，需要确认沙箱内是否存在对应的环境变量，如果没有，需要联系玲珑团队处理。

## 15. 应用运行需要的库文件没找到，如何提供？

应用需要使用的资源文件，与库文件需要应用自身提供。库文件放到files/lib路径下。

## 16. 在多用户时，应用激活相关私有数据存放到哪里？

v23设计了DSG_APP_DATA环境变量，应用私有数据存放到此路径下。

## 17. 应用修改了环境变量，导致应用启动失败。

应用启动脚修改沙箱环境变量时，注意验证其功能是否正常。

## 18. 应用下载目录可以选择哪里？

目前玲珑用户下载目录只能选择用户主目录下Desktop、Documents、Downloads、Music、Pictures、Videos、Public、Templates目录，不能下载到其他目录。

## 19. 打开文管操作如何执行？

打开文管时，采用xdg-open或者dbus打开，直接运行二进制会在沙箱内运行，由于沙箱会优先使用runtime内的qt库，如果runtime qt版本与系统不一致，会导致dde-file-manager调用失败。

## 20. 应用运行时，为什么QT WebEngine渲染进程已崩溃？

因v23升级了glibc，导致应用使用内置浏览器时失败，需要应用在v23环境适配。临时解决方案是设置环境变量：QTWEBENGINE_DISABLE_SANDBOX=1。
## 21. 应用运行时，找不到libqxcb.so库或者qtwebengin报错？
存在qt.conf文件时，在文件中配置正确路径，或者使用QTWEBENGINEPROCESS_PATH、QTWEBENGINERESOURCE_PATH、QT_QPA_PLATFORM_PLUGIN_PATH、QT_PLUGIN_PATH环境变量配置搜索路径。