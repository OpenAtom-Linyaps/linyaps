# ll-builder run
ll-builder 提供run命令供运行编译后的可执行程序。参数如下：

```plain
Usage: ll-builder [options] build

Options:
  -v, --verbose  show detail log
  -h, --help     Displays this help.
  --exec <exec>  run exec than build script

Arguments:
  run            run project
```
通过run命令， ll-builder将根据配置文件读取该程序相关的运行环境信息，为其构造一个沙箱环境，并在沙箱中执行该程序而无需安装。
为了便于调试，使用额外的参数可替换进入沙箱后默认执行的程序，如：

```plain
ll-builder --exec /bin/bash run
```
使用该选项，ll-builder 创建沙箱后将进入bash终端，可在沙箱内执行其他操作。
<!--TODO: Now the builder can not found the right exec path, and some environment is bot work, you can create and loader script and use exec to test, see `linglong.yaml` Examples for more. -->
