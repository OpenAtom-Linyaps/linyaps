% ll-builder-build 1

## NAME

ll-builder-build - 构建如意玲珑项目

## SYNOPSIS

**ll-builder build** [*options*] [*command*...]

## DESCRIPTION

`ll-builder build` 命令用来构建如意玲珑应用。构建完成后，构建内容将自动提交到本地 `ostree` 缓存中。

lock 不记录构建架构。依赖引用锁定 channel、ID 和精确版本，并保留原声明中的显式架构；未指定架构时记录 `unknown`，构建时使用当前构建环境架构，与 `package.architecture` 指定的输出目标架构独立。Base 由 Runtime 推导时，同样遵循 Runtime 的 Base 声明。未指定架构的依赖可以随构建环境切换架构，复用同一份 lock；各架构必须能够使用同一套锁定源码、依赖版本和模块集合。缺少所选架构的锁定版本或模块时构建报错，不会自动选择其他版本。

普通构建优先复用 `linglong.lock` 中与当前源码声明匹配的条目。所有源码比较 `kind` 和 `url`；`file`、`archive` 和 `dsc` 还比较规范化后的 `digest`；`git` 还比较 `commit` 和 `submodules`，未指定 `submodules` 时按默认启用处理。非 Git 源码的 `commit` 和 `submodules` 不参与匹配。`name` 和 `version` 不参与匹配，输出名称使用当前项目配置。

新增或声明变化的源码会重新下载，即使缓存中已有相同摘要的内容；配置了 `digest` 时必须校验通过，之后生成新的锁定条目。离线构建要求源码已有匹配的锁定条目和有效缓存。使用 `--locked` 时，源码声明不匹配会直接报错。

Git 源码的 `commit` 可以填写分支名、标签或提交 SHA。lock 使用 `resolved.hash: git-commit:<SHA>` 记录确定的提交。YAML 声明不变时，即使远端分支或标签已经移动，构建仍使用锁定的 SHA；执行 `ll-builder update-lockfile` 才会重新解析该分支或标签。修改 YAML 中的分支名或标签则属于声明变化，普通构建会重新获取，`--locked` 构建会报错。

构建容器中的文件修改会跨构建保留。当 base/runtime 的实际完整引用（包括架构）或模块列表变化时，构建环境会自动重置；执行 `ll-builder clean` 也会清除它。构建脚本应自行生成所需的临时数据，不应依赖上一次构建留下的文件。每次构建都会根据当前 `buildext.apt.build_depends` 执行 APT 安装流程，该列表变化不会触发环境重置。

## OPTIONS

**-h, --help**
: 打印帮助信息并退出

**--help-all**
: 展开所有帮助

**-f, --file** _FILE_
: 指定当前工作目录下的项目配置文件。未指定时优先使用 `linglong.<当前架构>.yaml`，不存在时使用 `linglong.yaml`

**--offline**
: 仅使用本地源码缓存和依赖，不从远程获取它们；仍会准备和校验源码

**--locked**
: 要求项目 lock 存在且与项目声明一致；不能与 --skip-fetch-source 同时使用

**--skip-fetch-source**
: 跳过源码获取，原样使用现有的 `linglong/sources/`，适合构建手动修改的源码；项目 lock 和依赖仍按普通构建规则处理

**--skip-pull-depend**
: 跳过拉取依赖项

**--skip-run-container**
: 跳过运行容器

**--skip-commit-output**
: 跳过提交构建输出

**--skip-output-check**
: 跳过输出检查

**--skip-strip-symbols**
: 跳过剥离调试符号

**--isolate-network**
: 在隔离的网络环境中构建

**command** -- _COMMAND_ ...
: 进入容器执行命令而不是构建应用

## EXAMPLES

### 基本用法

在项目根目录（linglong.yaml 文件所在目录）构建应用：

```bash
ll-builder build
```

或使用 `--file` 参数指定当前项目目录下的配置文件：

```bash
ll-builder build --file linglong.custom.yaml
```

进入容器执行命令而不是构建应用：

```bash
ll-builder build -- bash
```

## SEE ALSO

**[ll-builder(1)](./ll-builder.md)**, **[ll-builder-export(1)](export.md)**

## HISTORY

2023年，由 UnionTech Software Technology Co., Ltd. 开发
