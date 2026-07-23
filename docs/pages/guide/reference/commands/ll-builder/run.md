% ll-builder-run 1

## NAME

ll-builder-run - 运行构建好的应用程序

## SYNOPSIS

**ll-builder run** [*options*] [*command*...]

## DESCRIPTION

`ll-builder run` 命令根据配置文件读取该程序相关的运行环境信息，构造一个容器环境，并在容器中执行该程序而无需安装。

## OPTIONS

**-h, --help**
: 打印帮助信息并退出

**--help-all**
: 展开所有帮助

**-f, --file** _FILE_
: 指定当前工作目录下的项目配置文件。未指定时优先使用 `linglong.<当前架构>.yaml`，不存在时使用 `linglong.yaml`

**--modules** _modules_...
: 运行指定模块。例如: --modules binary,develop

**--workdir** _PATH_
: 指定应用程序运行的工作目录

**--debug**
: 在调试模式下运行（启用开发模块）

**--extensions** _extensions_
: 指定应用运行时使用的扩展

**COMMAND** _TEXT_...
: 进入容器执行命令而不是运行应用

## EXAMPLES

### 基本用法

运行构建好的应用程序， 进入项目目录：

```bash
ll-builder run
```

### 调试模式

为了便于调试，可以进入容器执行 shell 命令而不是运行应用：

```bash
ll-builder run /bin/bash
```

使用该命令，`ll-builder` 创建容器后将进入 `bash` 终端，可在容器内执行其他操作。

### 运行特定模块

运行指定的模块而不是默认的 binary 模块：

```bash
ll-builder run --modules binary,develop
```

### 使用扩展运行应用

运行应用时使用指定的扩展：

```bash
ll-builder run --extensions org.deepin.extension1,org.deepin.extension2
```

## SEE ALSO

**[ll-builder(1)](./ll-builder.md)**

## HISTORY

2023年，由 UnionTech Software Technology Co., Ltd. 开发
