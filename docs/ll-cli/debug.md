# 调试
## 与沙箱交互

使用ps命令可以看到所有正在linglong沙箱内运行的应用程序，包含containner-id，pid等信息：

```plain
ll-cli ps
```
如果有需要，可以根据containner-id强制终止一个程序的运行：
```plain
ll-cli kill <containner-id>
```
使用appid为指定程序创建一个沙箱，并获取到沙箱内的shell：
```plain
ll-cli run org.deepin.music --exec /bin/bash
```
## 运行调试工具

由于linglong应用都是在沙箱内运行，无法通过常规的方式直接调试，需要在沙箱内运行调试工具，如gdb：

```plain
gdb /opt/apps/org.deepin.music/files/bin/deepin-music
```
该路径为沙箱内应用程序的绝对路径。
