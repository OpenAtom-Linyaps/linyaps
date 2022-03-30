# ll-builder export
ll-builder 提供export命令生成bundle格式软件包（简称 uab）。参数如下：

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

通过export命令，ll-builder将从本地缓存中检出构建内容到当前目录，并根据该内容制作出bundle格式软件包。

```bash
$ ll-builder export
$ ls

export linglong.yaml org.deepin.demo_1.0.0_x86_64.uab
```
