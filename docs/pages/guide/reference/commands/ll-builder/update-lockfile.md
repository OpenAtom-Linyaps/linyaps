% ll-builder-update-lockfile 1

## NAME

ll-builder-update-lockfile - 更新构建资源 lock 文件

## SYNOPSIS

**ll-builder update-lockfile** [*options*]

## DESCRIPTION

`ll-builder update-lockfile` 根据当前项目 YAML 重新解析全部源码和 Base/Runtime 依赖，在资源准备成功后一次性替换对应的资源 lock 文件。即使资源声明没有变化，也会重新解析 Git 分支、标签和依赖版本要求；YAML 中的 `digest` 仍是必须满足的校验条件。此命令不执行项目构建脚本。

对应的 lock 文件必须已存在；不存在时命令报错，应先使用 `ll-builder generate-lockfile`。更新失败时保留旧 lock，但已获取的内容可以留在缓存中供后续使用。

lock 位于 YAML 入口旁，文件名由 YAML 的扩展名替换为 `.lock`。文件软链接和项目互斥锁的规则见 [ll-builder(1)](./ll-builder.md)。

更新后的 `build_architecture` 记录当前构建环境架构。将项目移到另一个构建架构的环境时，可以使用此命令重新锁定资源；它与 `package.architecture` 指定的输出目标架构独立。

普通构建会复用声明未变化的锁定资源。需要主动刷新资源时使用此命令，并审阅、提交 lock 的变化后重新构建。

## OPTIONS

**-h, --help**
: 打印帮助信息并退出

**--help-all**
: 展开所有帮助

**-f, --file** _FILE_
: 指定当前工作目录内的项目 YAML 文件，也可以位于其子目录。未指定时优先使用 `linglong.<当前构建架构>.yaml`，不存在时使用 `linglong.yaml`

## EXAMPLES

更新已有资源 lock，审阅变化后构建：

```bash
ll-builder update-lockfile
git diff -- linglong.lock
ll-builder build --locked
```

更新子目录中的配置对应的 `config/app.lock`：

```bash
ll-builder update-lockfile -f config/app.yaml
```

## SEE ALSO

**[ll-builder(1)](./ll-builder.md)**, **[ll-builder-build(1)](./build.md)**, **[ll-builder-generate-lockfile(1)](./generate-lockfile.md)**

## HISTORY

2026年，由 UnionTech Software Technology Co., Ltd. 开发
