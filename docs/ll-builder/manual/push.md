# ll-builder push
ll-builder 提供push命令将bundle格式软件包推送至远程服务器。参数如下：

```plain
Usage: ll-builder [options] push <filePath>

Options:
  -v, --verbose  show detail log
  -h, --help     Displays this help.
  --force        force push true or false

Arguments:
  push           push build result to repo
  filePath       bundle file path
```

通过push命令， ll-builder将根据输入的文件路径，读取bundle格式软件包内容，将软件数据及bundle格式软件包传输到服务端。

```bash
ll-builder push <org.deepin.demo-1.0.0_x86_64.uab>
```
