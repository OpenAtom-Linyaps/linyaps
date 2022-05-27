# ll-builder create

`ll-builder create`命令用来来创建玲珑项目。

查看`ll-builder create`命令的帮助信息：

```bash
ll-builder create --help
```

`ll-builder create`命令的帮助信息如下：

```plain
Usage: ll-builder [options] create org.deepin.demo

Options:
  -v, --verbose  show detail log
  -h, --help     Displays this help.

Arguments:
  create         create build template project
  name           project name
```

`ll-builder create`命令根据输入的项目名称在当前目录创建对应的文件夹，同时生成构建所需的linglong.yaml模板文件。示例如下：

```bash
ll-builder create <org.deepin.demo>
```

模板文件如下：

```plain
org.deepin.demo/
└── linglong.yaml
```
