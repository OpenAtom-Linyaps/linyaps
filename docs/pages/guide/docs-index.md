# 如意玲珑文档索引

本文档记录所有文档的索引信息。

## 目录结构概览

```txt
docs/pages/guide/
├── building/                    # 构建相关文档
├── debug/                       # 调试相关文档
├── desktop-integration/         # 桌面集成文档
├── extra/                       # 额外文档
├── lessons/                     # 教程和示例
├── linyaps-devel/               # 开发者文档
├── publishing/                  # 发布相关文档
├── reference/                   # 参考文档
├── start/                       # 入门指南
└── tips-and-faq/               # 提示和常见问题
```

## 详细文档索引

### 入门指南 (start/)

| 文档名称               | 文件路径                        | 描述                                                      |
| ---------------------- | ------------------------------- | --------------------------------------------------------- |
| 概述                   | `start/whatis.md`               | 介绍如意玲珑的基本概念、优势以及与其他包管理工具的对比    |
| 安装如意玲珑           | `start/install.md`              | 详细说明在不同Linux发行版上安装如意玲珑的步骤             |
| 构建第一个如意玲珑应用 | `start/build_your_first_app.md` | 从源码构建如意玲珑包的完整教程，包含计算器示例和deb包转换 |
| 发布说明               | `start/release_note.md`         | 版本发布说明                                              |

### 构建相关 (building/)

| 文档名称         | 文件路径                           | 描述                                                       |
| ---------------- | ---------------------------------- | ---------------------------------------------------------- |
| 玲珑应用打包规范 | `building/linyaps_package_spec.md` | 详细的应用程序打包规范，包含命名规范、构建配置、目录结构等 |
| 构建配置文件简介 | `building/manifests.md`            | `linglong.yaml` 配置文件的详细说明和字段定义               |
| 模块管理         | `building/modules.md`              | 模块拆分和管理说明                                         |
| 多架构支持       | `building/multiarch.md`            | 多架构构建支持说明                                         |
| 演示示例         | `building/demo.md`                 | 构建演示示例                                               |

### 调试相关 (debug/)

| 文档名称         | 文件路径         | 描述                                                        |
| ---------------- | ---------------- | ----------------------------------------------------------- |
| 调试如意玲珑应用 | `debug/debug.md` | 使用gdb、vscode、Qt Creator等工具调试如意玲珑应用的详细指南 |

### 桌面集成 (desktop-integration/)

| 文档名称     | 路径                            | 描述                           |
| ------------ | ------------------------------- | ------------------------------ |
| 桌面集成说明 | `desktop-integration/README.md` | 如意玲珑与桌面环境集成相关文档 |

### 教程和示例 (lessons/)

| 文档名称       | 文件路径                          | 描述                         |
| -------------- | --------------------------------- | ---------------------------- |
| 基础笔记       | `lessons/basic-notes.md`          | 基础使用笔记                 |
| 构建Git补丁    | `lessons/build-git-patch.md`      | Git补丁构建教程              |
| 环境内构建     | `lessons/build-in-env.md`         | 在特定环境中构建的教程       |
| 离线源码构建   | `lessons/build-offline-src.md`    | 离线环境下的源码构建教程     |
| 使用工具链测试 | `lessons/test-with-toolchains.md` | 使用不同工具链进行测试的教程 |

### 开发者文档 (linyaps-devel/)

| 文档名称   | 文件路径                  | 描述                     |
| ---------- | ------------------------- | ------------------------ |
| 开发者文档 | `linyaps-devel/README.md` | 开发者相关文档（待补充） |

### 发布相关 (publishing/)

| 文档名称    | 文件路径                     | 描述                                       |
| ----------- | ---------------------------- | ------------------------------------------ |
| 仓库管理    | `publishing/repositories.md` | 如意玲珑仓库的基本概念、管理和发布更新说明 |
| UAB格式发布 | `publishing/uab.md`          | UAB格式发布说明                            |
| 镜像站点    | `publishing/mirrors.md`      | 镜像站点配置和管理                         |

### 额外文档 (extra/)

| 文档名称   | 文件路径                 | 描述           |
| ---------- | ------------------------ | -------------- |
| 单元测试   | `extra/unit-testing.md`  | 单元测试文档   |
| 包格式     | `extra/bundle-format.md` | 包格式文档     |
| 系统助手   | `extra/system-helper.md` | 系统助手文档   |
| 应用配置   | `extra/app-conf.md`      | 应用配置文档   |
| 根文件系统 | `extra/rootfs.md`        | 根文件系统文档 |
| 参考文档   | `extra/ref.md`           | 参考文档       |
| UAB构建    | `extra/uab-build.md`     | UAB构建文档    |

### 参考文档 (reference/)

#### 基础概念

| 文档名称     | 文件路径                      | 描述                                    |
| ------------ | ----------------------------- | --------------------------------------- |
| 基础概念介绍 | `reference/basic-concepts.md` | Base、Runtime、沙箱、仓库等核心概念介绍 |
| 运行时组件   | `reference/runtime.md`        | 运行时组件的详细介绍                    |
| 驱动程序     | `reference/driver.md`         | 驱动程序相关文档                        |

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

**ll-pica 命令**

- `reference/commands/ll-pica/ll-pica.md` - 主命令说明
- `reference/commands/ll-pica/install.md` - install子命令
- `reference/commands/ll-pica/ll-pica-adep.md` - adep子命令
- `reference/commands/ll-pica/ll-pica-convert.md` - convert子命令
- `reference/commands/ll-pica/ll-pica-init.md` - init子命令

**ll-pica-flatpak 命令**

- `reference/commands/ll-pica-flatpak/ll-pica-flatpak.md` - 主命令说明
- `reference/commands/ll-pica-flatpak/ll-pica-flatpak-convert.md` - convert子命令

### 提示和常见问题 (tips-and-faq/)

| 文档名称            | 文件路径                         | 描述                     |
| ------------------- | -------------------------------- | ------------------------ |
| 常见问题            | `tips-and-faq/faq.md`            | 常见运行时问题及解决方案 |
| ll-builder 常见问题 | `tips-and-faq/ll-builder-faq.md` | 构建工具常见问题         |
| ll-pica 常见问题    | `tips-and-faq/ll-pica-faq.md`    | 包转换工具常见问题       |

## 快速导航

### 新手入门

1. 阅读 [`start/whatis.md`](start/whatis.md) 了解基本概念
2. 按照 [`start/install.md`](start/install.md) 安装如意玲珑
3. 跟随 [`start/build_your_first_app.md`](start/build_your_first_app.md) 构建第一个应用

### 开发者参考

- 打包规范: [`building/linyaps_package_spec.md`](building/linyaps_package_spec.md)
- 配置文件: [`building/manifests.md`](building/manifests.md)
- 调试指南: [`debug/debug.md`](debug/debug.md)

### 命令参考

- 构建工具: [`reference/commands/ll-builder/`](reference/commands/ll-builder/ll-builder.md) 目录
- 命令行工具: [`reference/commands/ll-cli/`](reference/commands/ll-cli/ll-cli.md) 目录
- deb包转换工具: [`reference/commands/ll-pica/`](reference/commands/ll-pica/ll-pica.md) 目录
- AppImage转换工具: [`reference/commands/ll-appimage-convert/`](reference/commands/ll-appimage-convert/ll-appimage-convert.md) 目录
- Flatpak转换工具: [`reference/commands/ll-pica-flatpak/`](reference/commands/ll-pica-flatpak/ll-pica-flatpak.md) 目录

### 常见问题

- 运行时问题: [`tips-and-faq/faq.md`](tips-and-faq/faq.md)
- 构建问题: [`tips-and-faq/ll-builder-faq.md`](tips-and-faq/ll-builder-faq.md)
- 转换问题: [`tips-and-faq/ll-pica-faq.md`](tips-and-faq/ll-pica-faq.md)
