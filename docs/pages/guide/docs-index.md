# 如意玲珑文档索引

本文档记录中文 `guide` 目录当前纳入导航大纲的文档。文档按读者角色分为“入门指南”“用户手册”和“开发者手册”；文件仍按内容类型存放。

## 目录结构

```txt
docs/pages/guide/
├── building/                    # 构建相关文档
├── debug/                       # 调试相关文档
├── desktop-integration/         # 桌面集成文档
├── extra/                       # 额外文档
├── lessons/                     # 精选课程
├── linyaps-devel/               # 开发者文档
├── ai-enhanced-assistance/      # AI辅助工具文档
├── publishing/                  # 发布相关文档
├── reference/                   # 参考文档
├── start/                       # 入门指南
└── tips-and-faq/               # 提示与常见问题
```

## 文档大纲

### 入门指南

入门指南默认展开，帮助读者快速了解和使用如意玲珑。

| 文档名称 | 文件路径 | 内容概要 |
| --- | --- | --- |
| 概述 | `start/whatis.md` | 介绍如意玲珑的用途、特点、适用场景及文档阅读方式 |
| 快速上手 | `start/quick-start.md` | 说明如何准备 `ll-cli`，以及搜索、安装和运行应用 |
| 发布说明 | `start/release_note.md` | 汇总各版本的新功能、问题修复和其他重要变更 |

### 用户手册

用户手册默认折叠，介绍如意玲珑的安装、应用管理和高级运行管理功能。

| 层级 | 文档名称 | 文件路径 | 内容概要 |
| --- | --- | --- | --- |
| 基础 | 安装如意玲珑 | `start/install.md` | 介绍仓库类型及在不同 Linux 发行版中的安装方法 |
| 基础 | 管理应用 | `start/manage-apps-with-cli.md` | 介绍应用的安装、查看、升级、卸载及运行进程管理 |
| 高级 | 运行配置 | `extra/runtime_config.md` | 说明运行配置的位置、加载顺序、完整示例和字段含义 |
| 高级 | 管理运行时 | `start/manage-runtimes-with-cli.md` | 介绍 Runtime 的查看、清理、依赖分析、覆盖和强制卸载 |
| 高级 | 仓库管理 | `publishing/repositories.md` | 说明仓库配置以及添加、修改优先级和删除等管理操作 |

### 开发者手册

开发者手册默认折叠，覆盖应用构建、发布、桌面集成和高级开发主题。

| 层级 | 文档名称 | 文件路径 | 内容概要 |
| --- | --- | --- | --- |
| 基础 | 构建第一个如意玲珑应用 | `start/build_your_first_app.md` | 通过配置元数据、依赖、源码和构建脚本完成首个应用构建 |
| 基础 | ll-builder 工作流 | `start/ll-builder-workflow.md` | 介绍项目创建、配置、构建、调试验证、导出和推送流程 |
| 构建示例 | 构建配置解读 | `building/demo.md` | 逐段解读 `linglong.yaml` 的软件包信息、依赖、源码和构建配置 |
| 构建示例 | 从 deb 包转换 | `building/deb_conversion.md` | 演示如何解包 deb、整理文件、补充依赖并生成如意玲珑应用 |
| 基础 | UAB 格式发布 | `publishing/uab.md` | 介绍使用 UAB 格式发布应用的相关内容，正文待补充 |
| 基础 | 桌面集成指南 | `desktop-integration/README.md` | 介绍通过 Portals 等机制集成桌面环境，正文待补充 |
| 高级 | 调试如意玲珑应用 | `debug/debug.md` | 介绍调试示例的准备，以及使用 gdb 调试应用的方法 |
| 高级 | 模块管理 | `building/modules.md` | 说明模块拆分、模块文件、保留模块及模块的构建和安装 |
| 高级 | 多架构支持 | `building/multiarch.md` | 介绍支持的架构、项目配置、构建命令和交叉构建方法 |
| 高级 | 如意玲珑打包 Agents 工具集 | `ai-enhanced-assistance/linyaps-packaging-agents-toolset.md` | 介绍如意玲珑打包 Agents 工具集各个SKILLS的能力、使用方法 |

## 推荐阅读顺序

### 普通用户

1. 概述
2. 快速上手
3. 安装如意玲珑
4. 管理应用
5. 按需阅读运行配置、管理运行时和仓库管理

### 应用开发者

1. 概述
2. 构建第一个如意玲珑应用
3. ll-builder 工作流
4. 阅读构建配置解读或从 deb 包转换
5. 按需阅读发布、桌面集成和高级开发文档
