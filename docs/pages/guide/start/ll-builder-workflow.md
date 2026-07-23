<!--
SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# ll-builder 工作流

`ll-builder` 是如意玲珑的应用构建工具。它读取项目中的 `linglong.yaml`，准备应用依赖和源码，在隔离环境中执行构建，并将构建结果保存到本地仓库，供后续调试、验证和分发。

如果希望直接完成一次实际构建，请阅读[构建第一个如意玲珑应用](./build_your_first_app.md)。本章重点说明各命令在完整开发流程中的位置，以及 `ll-builder build` 的工作原理。

## 工作流概览

```text
创建项目 → 编写 linglong.yaml → 构建 → 调试与运行验证 → 导出或推送
   │              │              │           │              │
 create       元数据、依赖、    build    run --debug       export
              源码、构建脚本             run               push
```

| 阶段 | 主要工作 | 命令或参考 |
| --- | --- | --- |
| 创建项目 | 创建项目目录和 `linglong.yaml` 模板 | [`ll-builder create`](../reference/commands/ll-builder/create.md) |
| 编写配置 | 定义应用元数据、启动命令、Base、Runtime、源码和构建脚本 | [构建配置文件简介](../building/manifests.md) |
| 构建 | 拉取依赖和源码，在隔离环境中编译并提交产物 | [`ll-builder build`](../reference/commands/ll-builder/build.md) |
| 调试 | 进入包含调试环境的容器，使用调试器定位问题 | [调试如意玲珑应用](../debug/debug.md) |
| 运行验证 | 不安装应用，直接运行本地构建结果 | [`ll-builder run`](../reference/commands/ll-builder/run.md) |
| 分发 | 导出 UAB，或将构建结果推送到远程仓库 | [`ll-builder export`](../reference/commands/ll-builder/export.md)、[`ll-builder push`](../reference/commands/ll-builder/push.md) |

## 主要步骤

### 1. 创建项目

使用稳定的倒置域名应用 ID 创建项目：

```bash
ll-builder create org.example.demo
cd org.example.demo
```

`ll-builder create` 会生成 `linglong.yaml` 模板。也可以在已有源码目录中手动创建该文件。

### 2. 编写构建配置

在 `linglong.yaml` 中至少确认以下内容：

- `package`：应用 ID、名称、版本、类型和描述。
- `command`：应用安装后的启动命令。
- `base` 和可选的 `runtime`：构建及运行所需的基础环境。
- `sources`：源码地址、版本和固定的提交或校验值。
- `build`：编译并将产物安装到 `${PREFIX}` 的命令。

字段格式和完整示例见[构建配置文件简介](../building/manifests.md)。

### 3. 构建应用

在 `linglong.yaml` 所在目录执行：

```bash
ll-builder build
```

构建完成后，使用下面的命令查看已提交到本地仓库的结果：

```bash
ll-builder list
```

如果需要清理某个本地构建结果，可以使用 [`ll-builder remove`](../reference/commands/ll-builder/remove.md)。

### 4. 调试和运行验证

直接运行本地构建结果：

```bash
ll-builder run
```

需要进入容器排查依赖、路径或启动问题时，使用调试模式：

```bash
ll-builder run --debug -- bash
```

调试模式、GDB 和 IDE 的详细配置见[调试如意玲珑应用](../debug/debug.md)。

### 5. 导出或推送

本地验证通过后，可以导出用于离线分发的 UAB：

```bash
ll-builder export --ref main:org.example.demo/1.0.0.0/<arch>
```

请使用 `ll-builder list` 显示的完整 ref 替换示例参数。需要发布到远程仓库时，在配置仓库和凭据后执行：

```bash
ll-builder push
```

仓库配置和发布方式见[仓库管理](../publishing/repositories.md)。所有子命令及参数见 [`ll-builder` 命令参考](../reference/commands/ll-builder/ll-builder.md)。

## 理解 ll-builder 工作原理

可以把 `ll-builder build` 理解为一条自动化装配线：`linglong.yaml` 是装配说明，源码是待加工的材料，Base 和 Runtime 是统一的工作环境，最终产物则会被整理后存入本地仓库。

一次完整构建会依次经过以下阶段：

```text
读取配置 → 准备源码和依赖 → 创建隔离环境 → 执行构建 → 整理产物 → 保存并检查
```

### 读取配置

`ll-builder` 首先读取 `linglong.yaml`，确认应用 ID、版本、包类型、启动命令、Base、Runtime、源码和构建脚本等信息。如果必要字段缺失或格式不正确，构建会在进入容器前停止。

### 获取源码和依赖

`ll-builder` 根据 `sources` 下载或展开源码，并把它们准备到 `linglong/sources/`。构建容器中对应的路径是 `/project/linglong/sources/`。下载过的内容可以复用缓存，但每次正常构建都会重新准备源码目录，避免上一次构建修改过的文件影响结果。将获取源码放在单独的步骤而不是放到构建指令中，是为了使得构建过程可重复/可追溯/可审计。

随后会准备 `base` 和 `runtime`，用于创建构建环境。

### 创建隔离的构建环境

正式编译前，`ll-builder` 会：

1. 清理上一次构建的临时目录，比如 `linglong/output/`。需要注意 `linglong` 目录是 ll-builder 内部使用工作目录，用户不应该在此目录存放其他文件。
2. 将 Base、Runtime、项目目录、源码和输出目录组合成构建环境，并为 Base 和 Runtime 创建临时可写层，避免修改原始依赖。
3. 处理 `buildext.apt.buildDepends` 等构建期依赖的安装。

这个环境是与宿主机相互隔离的容器，但仍能访问当前项目和已经准备好的源码。容器内会提供两个常用变量：

```text
PREFIX=<当前包的安装前缀>
TRIPLET=<当前 CPU 架构的 GNU triplet>
```

然后，`ll-builder` 在容器中执行 `linglong.yaml` 的 `build` 脚本。脚本产生的可执行文件、库和资源应安装到 `${PREFIX}`。写入 `${PREFIX}` 的内容会进入最终软件包；直接写入 `/usr` 的内容只会存在于临时构建环境中，不会作为应用产物。

### 整理和保存构建结果

构建脚本成功结束后，`ll-builder` 会从 `${PREFIX}` 收集产物，生成应用配置，并按照 `modules` 规则拆分内容：

- `binary` 通常包含用户运行应用所需的文件。
- `develop` 通常包含头文件、静态库和调试信息。
- 自定义模块包含项目自行定义的可选内容。

桌面文件、图标和服务配置等需要与桌面环境集成的内容也会在此阶段单独整理。完成后，各模块会保存到本地 OSTree 仓库。

最后，App 会在声明的 Base 和 Runtime 组合中接受运行检查，尽早发现缺少动态库、文件位置错误等问题。构建成功的应用可以由 `ll-builder run` 直接运行，也可以继续导出或推送。

应用包包含自身需要交付的内容，运行应用时，这些内容会与 Base、Runtime 等运行时组件重新组合为 Rootless 容器中的文件系统。关于包类型、容器、OSTree 和工具之间的关系，参阅[基础概念介绍](../reference/basic-concepts.md)。

### build 参数对流程的影响

以下选项会改变构建输入或阶段执行方式。`-h, --help` 和 `--help-all` 只显示帮助并退出，不会进入构建流程。

| 参数 | 影响阶段 | 对构建过程的影响 |
| --- | --- | --- |
| `-f, --file <file>` | 读取配置 | 使用指定配置代替自动查找的 `linglong.<arch>.yaml` 或 `linglong.yaml`。文件必须位于当前项目目录或其子目录中 |
| `--offline` | 获取源码和依赖 | 不重新获取源码，也不从远程拉取 Base、Runtime；所需源码和依赖必须已经存在于本地。`linglong.yaml` 中的 `offline: true` 作用相同 |
| `--skip-fetch-source` | 获取源码 | 保留并使用现有的 `linglong/sources/`，不会清理或重新获取 `sources` |
| `--skip-pull-depend` | 获取依赖 | 不从远程拉取新内容，但仍会解析和合并本地已有的 Base、Runtime 模块；本地缺少依赖时构建失败 |
| `--skip-run-container` | 执行构建 | 仍会完成配置、源码和依赖准备，但不会清理输出、创建构建容器或执行构建脚本；命令随后结束，不再整理和检查输出 |
| `--skip-commit-output` | 保存产物 | 正常执行容器构建，但不生成模块、桌面集成内容或应用配置，也不保存到本地仓库；随后不执行运行检查 |
| `--skip-output-check` | 检查产物 | 仍会执行检查并收集导出 UAB 所需的信息，但检查失败不会使本次构建失败 |
| `--skip-strip-symbols` | 执行构建 | 关闭 `ll-builder` 自动执行的符号分离及剥离流程；最终是否包含调试信息仍取决于项目自身的编译选项 |
| `--isolate-network` | 执行构建 | 只断开执行 `build` 脚本的主构建容器网络。源码获取和 Base、Runtime 拉取发生在此之前，不受该选项影响 |
| `COMMAND...` | 执行构建 | 使用指定命令代替 `linglong.yaml` 中的 `build` 脚本，适合在相同环境中排查问题或手动构建。命令成功退出后仍会整理和检查产物；只做诊断时可配合 `--skip-commit-output` |

`--skip-run-container` 和 `--skip-commit-output` 都会提前结束后续流程，因此它们适合分阶段排查问题，不代表已经生成了可供 `ll-builder run` 使用的新构建结果。参数的命令行定义见 [`ll-builder build`](../reference/commands/ll-builder/build.md)。
