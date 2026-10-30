% ll-builder-generate-lockfile 1

## NAME

ll-builder-generate-lockfile - 生成构建资源 lock 文件

## SYNOPSIS

**ll-builder generate-lockfile** [*options*]

## DESCRIPTION

`ll-builder generate-lockfile` 获取项目声明的源码和 Base/Runtime 依赖，记录源码摘要、Git commit、依赖精确引用及模块列表，在资源准备成功后生成完整的资源 lock 文件。此命令不执行项目构建脚本。

对应的 lock 文件必须不存在；已存在时命令报错，应使用 `ll-builder update-lockfile` 更新。资源准备失败时不会生成完整 lock，但已获取的内容可以留在缓存中供后续使用。

lock 位于 YAML 入口旁，文件名由 YAML 的扩展名替换为 `.lock`，例如 `linglong.yaml` 对应 `linglong.lock`，`config/app.yaml` 对应 `config/app.lock`。文件软链接和项目互斥锁的规则见 [ll-builder(1)](./ll-builder.md)。

应将生成的 lock 与项目配置一起提交到版本控制，后续可使用 `ll-builder build --locked` 按锁定资源构建。

## OPTIONS

**-h, --help**
: 打印帮助信息并退出

**--help-all**
: 展开所有帮助

**-f, --file** _FILE_
: 指定当前工作目录内的项目 YAML 文件，也可以位于其子目录。未指定时优先使用 `linglong.<当前构建架构>.yaml`，不存在时使用 `linglong.yaml`

## EXAMPLES

在项目尚无 lock 时生成资源 lock，然后构建：

```bash
ll-builder generate-lockfile
ll-builder build --locked
```

为子目录中的配置生成对应的 `config/app.lock`：

```bash
ll-builder generate-lockfile -f config/app.yaml
```

## SEE ALSO

**[ll-builder(1)](./ll-builder.md)**, **[ll-builder-build(1)](./build.md)**, **[ll-builder-update-lockfile(1)](./update-lockfile.md)**

## HISTORY

2026年，由 UnionTech Software Technology Co., Ltd. 开发
