# uab 构建

从面向玲珑构建的项目中创建出一个能够运行的uab文件需要以下几个配置文件：

1. linglong.yaml：玲珑项目的配置文件，决定了该包如何被编译和安装；
2. loader：一个可执行文件，作为uab的入口。
3. extra-files.txt：一个文本文件，其中写明该应用依赖base和runtime中的哪些文件，
   这些文件会被拷贝到uab包中，应用运行时放置在相应位置。如果应用并不依赖runtime
   以及base中的其他文件就可以运行起来，则可以不提供该文件。

如果只是将项目导出成uab用来向玲珑商店提交应用，并不需要其能够运行，并不需要提供
loader和extra-files.txt

loader的编写参考：

```bash
#!/bin/env sh
APPID=org.deepin.demo
./ll-box $APPID $PWD /opt/apps/$APPID/files/bin/demo
```

传递给ll-box的最后一个参数为需要在容器中运行的二进制。
