# ll-builder create
ll-builder 提供了create命令来创建一个项目。如下：

```plain
Usage: ll-builder [options] create <org.deepin.demo>

Options:
  -v, --verbose  show detail log
  -h, --help     Displays this help.

Arguments:
  create         create build template project
  name           project name
```

通过create命令， ll-builder将根据输入的项目名称在当前目录创建一个对应的文件夹，同时生成构建所需的linglong.yaml模板文件。

```plain
org.deepin.demo/
└── linglong.yaml
```


