# 如意玲珑文档索引

本文档记录所有文档的索引信息。

## 目录结构概览

```
docs/pages/guide/
├── building/                    # 构建相关文档
├── debug/                       # 调试相关文档
├── desktop-integration/         # 桌面集成文档
├── lessons/                     # 教程和示例
├── linyaps-devel/               # 开发者文档
├── publishing/                  # 发布相关文档
├── reference/                   # 参考文档
└── start/                       # 入门指南
```

## 详细文档索引

### 入门指南 (start/)

| 文档名称 | 文件路径 | 描述 |
|---------|---------|------|
| 概述 | `start/whatis.md` | 介绍如意玲珑的基本概念、优势以及与其他包管理工具的对比 |
| 安装如意玲珑 | `start/install.md` | 详细说明在不同Linux发行版上安装如意玲珑的步骤 |
| 构建第一个如意玲珑应用 | `start/build_your_first_app.md` | 从源码构建如意玲珑包的完整教程，包含计算器示例和deb包转换 |
| 发布说明 | `start/release_note.md` | 版本发布说明 |

### 构建相关 (building/)

| 文档名称 | 文件路径 | 描述 |
|---------|---------|------|
| 玲珑应用打包规范 | `building/linyaps_package_spec.md` | 详细的应用程序打包规范，包含命名规范、构建配置、目录结构等 |
| 构建配置文件简介 | `building/manifests.md` | `linglong.yaml` 配置文件的详细说明和字段定义 |
| 模块管理 | `building/modules.md` | 模块拆分和管理说明 |
| 多架构支持 | `building/multiarch.md` | 多架构构建支持说明 |
| 演示示例 | `building/demo.md` | 构建演示示例 |

### 调试相关 (debug/)

| 文档名称 | 文件路径 | 描述 |
|---------|---------|------|
| 调试如意玲珑应用 | `debug/debug.md` | 使用gdb、vscode、Qt Creator等工具调试如意玲珑应用的详细指南 |
| 常见问题解答 | `debug/faq.md` | 常见问题解答 |
| ll-builder常见问题 | `debug/ll-builder-faq.md` | 构建工具常见问题 |
| ll-pica常见问题 | `debug/ll-pica-faq.md` | 包转换工具常见问题 |

### 桌面集成 (desktop-integration/)

| 文档名称 | 文件路径 | 描述 |
|---------|---------|------|
| 桌面集成说明 | `desktop-integration/README.md` | 如意玲珑与桌面环境集成相关文档说明 |

### 教程和示例 (lessons/)

| 文档名称 | 文件路径 | 描述 |
|---------|---------|------|
| 基础笔记 | `lessons/basic-notes.md` | 基础使用笔记 |
| 构建Git补丁 | `lessons/build-git-patch.md` | Git补丁构建教程 |
| 环境内构建 | `lessons/build-in-env.md` | 在特定环境中构建的教程 |
| 离线源码构建 | `lessons/build-offline-src.md` | 离线环境下的源码构建教程 |
| 使用工具链测试 | `lessons/test-with-toolchains.md` | 使用不同工具链进行测试的教程 |

### 开发者文档 (linyaps-devel/)

| 文档名称 | 文件路径 | 描述 |
|---------|---------|------|
| 开发者文档 | `linyaps-devel/README.md` | 开发者相关文档（待补充） |

### 发布相关 (publishing/)

| 文档名称 | 文件路径 | 描述 |
|---------|---------|------|
| 仓库管理 | `publishing/repositories.md` | 如意玲珑仓库的基本概念、管理和发布更新说明 |
| UAB格式发布 | `publishing/uab.md` | UAB格式发布说明 |

### 参考文档 (reference/)

#### 基础概念

| 文档名称 | 文件路径 | 描述 |
|---------|---------|------|
| 基础概念介绍 | `reference/basic-concepts.md` | Base、Runtime、沙箱、仓库等核心概念介绍 |
| 运行时组件 | `reference/runtime.md` | Runtime组件的详细介绍 |

#### 命令参考

**ll-appimage-convert 命令**

- `reference/commands/ll-appimage-convert/ll-appimage-convert.md` - 主命令说明
- `reference/commands/ll-appimage-convert/ll-appimage-convert-convert.md` - convert子命令

**ll-builder 命令**

- `reference/commands/ll-builder/ll-builder.md` - 主命令说明
- `reference/commands/ll-builder/build.md` - build子命令
- `reference/commands/ll-builder/create.md` - create子命令
- `reference/commands/ll-builder/export.md` - export子命令
- `reference/commands/ll-builder/extract.md` - extract子命令
- `reference/commands/ll-builder/import.md` - import子命令
- `reference/commands/ll-builder/list.md` - list子命令
- `reference/commands/ll-builder/push.md` - push子命令
- `reference/commands/ll-builder/remove.md` - remove子命令
- `reference/commands/ll-builder/repo.md` - repo子命令
- `reference/commands/ll-builder/run.md` - run子命令

**ll-cli 命令**

- `reference/commands/ll-cli/ll-cli.md` - 主命令说明
- `reference/commands/ll-cli/content.md` - content子命令
- `reference/commands/ll-cli/enter.md` - enter子命令
- `reference/commands/ll-cli/info.md` - info子命令
- `reference/commands/ll-cli/install.md` - install子命令
- `reference/commands/ll-cli/kill.md` - kill子命令
- `reference/commands/ll-cli/list.md` - list子命令
- `reference/commands/ll-cli/prune.md` - prune子命令
- `reference/commands/ll-cli/ps.md` - ps子命令
- `reference/commands/ll-cli/repo.md` - repo子命令
- `reference/commands/ll-cli/run.md` - run子命令
- `reference/commands/ll-cli/search.md` - search子命令
- `reference/commands/ll-cli/uninstall.md` - uninstall子命令
- `reference/commands/ll-cli/upgrade.md` - upgrade子命令

**ll-flatpak-convert 命令**

- `reference/commands/ll-flatpak-convert/ll-flatpak-convert.md` - 主命令说明
- `reference/commands/ll-flatpak-convert/ll-flatpak-convert-convert.md` - convert子命令

**ll-pica 命令**

- `reference/commands/ll-pica/ll-pica.md` - 主命令说明
- `reference/commands/ll-pica/install.md` - install子命令
- `reference/commands/ll-pica/ll-pica-adep.md` - adep子命令
- `reference/commands/ll-pica/ll-pica-convert.md` - convert子命令
- `reference/commands/ll-pica/ll-pica-init.md` - init子命令

## 快速导航

### 新手入门

1. 阅读 [`start/whatis.md`](../guide/start/whatis.md) 了解基本概念
2. 按照 [`start/install.md`](../guide/start/install.md) 安装如意玲珑
3. 跟随 [`start/build_your_first_app.md`](../guide/start/build_your_first_app.md) 构建第一个应用

### 开发者参考

- 打包规范: [`building/linyaps_package_spec.md`](../guide/building/linyaps_package_spec.md)
- 配置文件: [`building/manifests.md`](../guide/building/manifests.md)
- 调试指南: [`debug/debug.md`](../guide/debug/debug.md)

### 命令参考

- 构建工具: [`reference/commands/ll-builder/`](../guide/reference/commands/ll-builder/) 目录
- 命令行工具: [`reference/commands/ll-cli/`](../guide/reference/commands/ll-cli/) 目录
- deb包转换工具: [`reference/commands/ll-pica/`](../guide/reference/commands/ll-pica/) 目录
- appimage转换工具: [`reference/commands/ll-appimage-convert/`](../guide/reference/commands/ll-appimage-convert/) 目录
- flatpak转换工具: [`reference/commands/ll-flatpak-convert/`](../guide/reference/commands/ll-flatpak-convert/) 目录
