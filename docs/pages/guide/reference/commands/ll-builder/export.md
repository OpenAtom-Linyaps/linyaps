% ll-builder-export 1

## NAME

ll-builder-export - 导出如意玲珑 layer 或 UAB 文件

## SYNOPSIS

**ll-builder export** [*options*]

## DESCRIPTION

`ll-builder export` 默认将本地构建缓存中的包导出为用于安装和分发的 distribution 模式 UAB。使用 `--uabx` 可生成只包含应用自身、可直接执行的 UABX。也可以导出为已弃用的 linglong layer 文件格式。

## OPTIONS

**-h, --help**
: 打印帮助信息并退出

**--help-all**
: 展开所有帮助

**-f, --file** _FILE_
: 指定当前工作目录下的项目配置文件。未指定时优先使用 `linglong.<当前架构>.yaml`，不存在时使用 `linglong.yaml`

**-o, --output** _file_
: 指定 UAB 输出文件的路径，与 `--layer` 互斥

**-z, --compressor** _x_
: 指定压缩算法。支持 `lz4` (UAB 默认), `lzma` (layer 默认), `zstd`

**--icon** _file_
: 为导出的 UABX 指定图标，必须与 `--uabx` 一起使用

**--uabx**
: 导出 exec 模式 UABX，仅包含目标应用自身的模块和 loader，不包含 Base、Runtime、`uab-loader` 或 `ll-box`

**--loader** _file_
: 为导出的 UABX 指定自定义 loader，必须与 `--uabx` 一起使用；未指定时根据包的 `command` 在应用根目录生成 `entry.sh`，并生成指向该入口的默认 loader

**--layer**
: **(已弃用)** 导出为 layer 文件格式，而不是 UAB；与 `--icon`、`--loader`、`--output`、`--ref` 和 `--modules` 互斥

**--no-develop**
: 在导出 layer 文件时不导出 `develop` 模块，必须与 `--layer` 一起使用

**--ref** _ref_
: 指定要导出的包引用；未指定时根据当前项目查找引用，与 `--layer` 互斥

**--modules** _modules_
: 指定要导出到 UAB 的模块，多个模块用逗号分隔，与 `--layer` 互斥

## EXAMPLES

### 导出 distribution 模式 UAB (推荐)

distribution 模式 UAB 包含目标 ref 的所选模块，安装时由包管理器补全依赖。

```bash
# 基本导出
ll-builder export

# 导出指定 ref
ll-builder export --ref main:org.example.demo/1.0.0.0/x86_64

# 导出 UAB 文件并使用 zstd 压缩
ll-builder export -z zstd -o my-app-zstd.uab

```

### 导出 exec 模式 UABX

```bash
# 根据当前项目导出并生成默认 loader
ll-builder export --uabx

# 导出指定 ref 并嵌入图标
ll-builder export --uabx --ref main:org.example.demo/1.0.0.0/x86_64 \
  --icon assets/app.png

# 使用自定义 loader
ll-builder export --uabx --loader /path/to/custom/loader -o my-app.uabx
```

### 导出 layer 文件

```bash
# 导出 layer 格式，且不包含 develop 模块
ll-builder export --layer --no-develop

# 导出 layer 格式
ll-builder export --layer
```

## 进阶说明

UAB 和 UABX 都使用 ELF header 加 EROFS bundle 的格式，但用途和内容不同。

### Distribution 模式

默认的 distribution 模式仅封装指定 ref 的模块，不带 loader。通常通过 `ll-cli install <UABfile>` 安装，并由包管理器补全应用依赖。

### Exec 模式

`--uabx` 导出的 exec 模式仅包含应用自身模块及 loader。未指定 `--loader` 时，会根据包的 `command` 在 `LINGLONG_UAB_APPROOT` 中生成 `entry.sh`，根目录 loader 会执行该入口；指定 `--loader` 时则直接使用自定义 loader。UABX 适合直接执行，不保证支持 `ll-cli install`。

## SEE ALSO

**[ll-builder(1)](./ll-builder.md)**

## HISTORY

2023年，由 UnionTech Software Technology Co., Ltd. 开发
