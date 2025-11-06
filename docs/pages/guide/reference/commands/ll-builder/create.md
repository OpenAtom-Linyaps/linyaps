% ll-builder-create 1

## NAME

ll-builder-create - 创建如意玲珑构建模板

## SYNOPSIS

**ll-builder create** [*options*] _name_

## DESCRIPTION

`ll-builder create` 命令根据输入的项目名称在当前目录创建对应的文件夹，同时生成构建所需的 `linglong.yaml` 模板文件。

## OPTIONS

**-h, --help**
: 打印帮助信息并退出

**--help-all**
: 展开所有帮助

**name** (必需)
: 项目名称

## EXAMPLES

创建名为 `org.deepin.demo` 的项目：

```bash
ll-builder create org.deepin.demo
```

命令输出如下：

```text
org.deepin.demo/
└── linglong.yaml
```

### 完整的 linglong.yaml 配置

`linglong.yaml` 文件内容如下：

```yaml
version: "1"

package:
  id: org.deepin.demo
  name: your name #set your application name
  version: 0.0.0.1 #set your version
  kind: app
  description: |
    your description #set a brief text to introduce your application.

command: [echo, -e, hello world] #the commands that your application need to run.

base: org.deepin.base/23.1.0 #set the base environment, this can be changed.

#set the runtime environment if you need, a example of setting deepin runtime is as follows.
#runtime:
#org.deepin.runtime.dtk/23.1.0

#set the source if you need, a simple example of git is as follows.
#sources:
#  - kind: git
#    url: https://github.com/linuxdeepin/linglong-builder-demo.git
#    version: master\n
#    commit: a3b89c3aa34c1aff8d7f823f0f4a87d5da8d4dc0

build: |
  echo 'hello' #some operation to build this project
```

## SEE ALSO

**[ll-builder(1)](./ll-builder.md)**

## HISTORY

2023年，由 UnionTech Software Technology Co., Ltd. 开发
