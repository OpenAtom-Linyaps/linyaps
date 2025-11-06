% ll-builder-create 1

## NAME

ll-builder-create - Create Linyaps build template

## SYNOPSIS

**ll-builder create** [*options*] _name_

## DESCRIPTION

The `ll-builder create` command creates a corresponding folder in the current directory based on the input project name, and generates the required `linglong.yaml` template file for building.

## OPTIONS

**-h, --help**
: Print help information and exit

**--help-all**
: Expand all help

**name** (required)
: Project name

## EXAMPLES

Create a project named `org.deepin.demo`:

```bash
ll-builder create org.deepin.demo
```

Command output as follows:

```text
org.deepin.demo/
└── linglong.yaml
```

### Complete linglong.yaml configuration

The `linglong.yaml` file content is as follows:

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

Developed in 2023 by UnionTech Software Technology Co., Ltd.
