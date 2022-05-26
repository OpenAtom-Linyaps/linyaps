# ll-builder export
ll-builder 提供export命令检出构建内容, 生成bundle格式软件包（简称 uab）。参数如下：

```plain
Usage: ll-builder [options] export [filename]

Options:
  -v, --verbose  show detail log
  -h, --help     Displays this help.

Arguments:
  export         export build result to uab bundle
  filename       bundle file name , if filename is empty,export default format
                 bundle
```

通过export命令，ll-builder将在工程根目录下创建以appid为名称的目录，并将本地构建缓存检出到该目录。同时根据该构建结果制作出bundle格式软件包。

```bash
ll-builder export
```

```bash
ls
```

```plain
org.deepin.demo linglong.yaml org.deepin.demo_1.0.0_x86_64.uab
```