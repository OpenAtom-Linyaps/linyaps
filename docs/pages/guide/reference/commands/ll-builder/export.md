% ll-builder-export 1

## NAME

ll-builder-export - 导出如意玲珑 layer 或 UAB 文件

## SYNOPSIS

**ll-builder export** [*options*]

## DESCRIPTION

`ll-builder export` 命令用于将本地构建缓存中的应用导出为 UAB (Universal Application Bundle) 文件。这是推荐的格式。也可以导出为已弃用的 linglong layer 文件格式。

## OPTIONS

**-h, --help**
: 打印帮助信息并退出

**--help-all**
: 展开所有帮助

**-f, --file** _file_ [./linglong.yaml]
: 指定 `linglong.yaml` 配置文件的路径，`linglong.yaml` 文件所在的目录为项目的工作目录

**-o, --output** _file_
: 指定输出文件的路径。对于 UAB，这通常是 `.uab` 文件的完整路径或文件名。对于 layer 这是输出文件名的前缀

**-z, --compressor** _x_
: 指定压缩算法。支持 `lz4` (UAB 默认), `lzma` (layer 默认), `zstd`

**--icon** _file_
: 为导出的 UAB 文件指定图标 (仅 UAB 模式，与 `--layer` 互斥)

**--loader** _file_
: 为导出的 UAB 文件指定自定义加载器 (仅 UAB 模式，与 `--layer` 互斥)

**--layer**
: **(已弃用)** 导出为 layer 文件格式，而不是 UAB (与 `--icon`, `--loader` 互斥)

**--no-develop**
: 在导出 layer 文件时，不导出 `develop` 模块

**--ref** _ref_
: 指定包的引用

**--modules** _modules_
: 指定要导出的模块

## EXAMPLES

### 导出 UAB 文件 (推荐)

UAB (Universal Application Bundle) 文件一种自包含、可离线分发的文件格式，包含了应用运行所需的内容。这是推荐的导出格式。

```bash
# 基本导出
ll-builder export

# 导出 UAB 文件并指定图标和输出路径
ll-builder export --icon assets/app.png -o dist/my-app-installer.uab

# 导出 UAB 文件并使用 zstd 压缩
ll-builder export -z zstd -o my-app-zstd.uab

# 导出 UAB 文件并指定自定义加载器
ll-builder export --loader /path/to/custom/loader -o my-app-custom-loader.uab
```

### 导出 layer 文件

```bash
# 导出 layer 格式，且不包含 develop 模块
ll-builder export --layer --no-develop

# 导出 layer 格式，并指定输出文件前缀
ll-builder export --layer -o my-app
# (会生成 my-app_binary.layer 和 my-app_develop.layer)
```

## 进阶说明

UAB 是一个静态链接的 ELF 可执行文件，其目标是可以在任意 Linux 发行版上运行。默认情况下导出 UAB 文件属于 bundle 模式，而使用参数 `--loader` 导出 UAB 文件属于 custom loader 模式。

### Bundle 模式

bundle 模式主要用于分发，并尽可能的支持自运行功能。通常用户通过 `ll-cli install <UABfile>` 安装离线分发的应用，并会自动补全应用所需的运行环境。

### Custom Loader 模式

custom loader 模式导出的 UAB 文件仅包含应用数据，以及传入的 custom loader。UAB 文件在解压挂载后将控制器交给 custom loader，此时 loader 不在容器环境内。

## SEE ALSO

**[ll-builder(1)](./ll-builder.md)**

## HISTORY

2023年，由 UnionTech Software Technology Co., Ltd. 开发
