# 如意玲珑打包 Agents 工具集

## Toolset 简介

linyaps-packaging-agents-toolset 是一个面向如意玲珑（Linyaps）打包工程的 Agents 工具集，覆盖从构建规则生成、便捷打包脚本创建到工程模板执行的完整链路。配套 `deepin 25` / `统信桌面操作系统 V25 专业版` 使用效果更佳。

内置三个 Agents 及其职责：

| Agent | 版本 | 职责 |
|-------|------|------|
| debian-rules-to-linyaps-rules | v2026.7.20.1 | 分析项目源码的 Debian 构建规则，生成玲珑构建配置 `linglong.yaml` |
| linyaps-app-packaging-script-generator | v2026.7.20.1 | 将 deb / tar 归档 / AppImage 批量转换为玲珑便捷打包脚本工程 |
| linyaps-packaging-scripts-running-skill | v2026.7.20.1 | 基于已有打包工程模板，接受新版来源执行构建，产出 layer 包 |

## 安装部署

### 系统要求

- 操作系统：`deepin 25` 或 `统信桌面操作系统 V25 专业版`
- CPU 架构：x86_64 / arm64 （按实际环境选择）

### 安装依赖包

三个 Agent 均依赖以下软件包，需通过 `apt` 安装：

```bash
sudo apt install python3-yaml python3-ruamel.yaml
```

从对应 Agent 仓库的 `Release` 页面下载指定版本的 `linglong-bin` 和 `linglong-builder` 后进行升级/降级：

```bash
sudo apt install ./linglong-bin_1.13.7-ziggy2_amd64.deb ./linglong-builder_1.13.7-ziggy2_amd64.deb
```
\* 需要安装的文件和路径以实际为准

### 导入 Agents 与 Skills

根据所使用的 Coding Agent CLI 工具，参考对应文档导入 agents 配置和 skills 目录：

| 工具 | Agents 导入 | Skills 导入 |
|------|-------------|-------------|
| OpenCode | [查看文档](https://opencode.ai/docs/zh-cn/agents/#markdown) | [查看文档](https://opencode.ai/docs/zh-cn/skills/) |
| Codex | [查看文档](https://learn.chatgpt.com/docs/customization/overview#agents-guidance) | [查看文档](https://learn.chatgpt.com/docs/customization/overview#skills) |
| VS Code | [查看文档](https://code.visualstudio.com/docs/agent-customization/custom-agents) | [查看文档](https://code.visualstudio.com/docs/agent-customization/agent-skills) |
| TRAE IDE | [查看文档](https://docs.trae.ai/ide/subagents?_lang=zh) | [查看文档](https://docs.trae.ai/ide/skills?_lang=zh) |

## Agents

### debian-rules-to-linyaps-rules

- **仓库地址**：<https://github.com/OpenAtom-Linyaps/debian-rules-to-linyaps-rules>

#### 简介

分析项目源代码对应的 Debian 构建规则和构建资源，为玲珑（Linyaps）构建项目生成构建、编译规则。产出的 `linglong.yaml` 包含完整的构建规则段（build section），可用于辅助玲珑构建。

#### 建议使用场景

- 项目已提供 Debian 构建规则（`debian/` 目录），需转译为玲珑构建配置
- 不熟悉 Linux 打包规范和 `linglong.yaml` 手工编写的入门用户
- 希望复用现有 Debian 生态构建资源，加速玲珑适配过程

#### skills 能力介绍

| Skill 名称 | 功能描述 |
|------------|----------|
| `src2linyaps.debian.analyze-control` | 解析 `debian/control` 文件，提取源码包名、构建依赖列表、描述信息等结构化数据；处理多个 Package 条目的全量合并去重；基于 apt 仓库解析构建依赖的运行时依赖 |
| `src2linyaps.debian.analyze-rules` | 分析 debian 构建规则和资源文件，输出构建工具类型、编译参数及默认值、baseline 版本、源码包名、合并后资源列表、build_section 的最终 YAML |
| `src2linyaps.debian.build-res-generate` | 基于 analyze-control 输出的依赖信息和 analyze-rules 输出的 build 段，结合模板和默认值配置，生成完整的 `linglong.yaml` 玲珑构建规则 |
| `src2linyaps.debian.test-deps` | 读取 `debian/control` 中的 `Build-Depends` 字段，通过 `apt-get build-dep --dry-run` 模拟安装检测依赖可用性，输出依赖包状态：可用、部分缺失或完全不可用 |
| `src2linyaps.source.analyze-args` | 根据构建工具类型读取对应的构建配置文件，解析可修改的编译参数（如 prefix、DESTDIR、CMAKE_INSTALL_PREFIX 等），输出构建工具类型和编译参数列表 |
| `src2linyaps.source.detect-tool` | 扫描项目源码根目录，按优先级匹配特征文件，检测项目使用的构建工具类型 |

#### 建议提示词

```text
https://linux.apps.demo.com/download/demo.orig.tar.xz 是一个开源项目源码包，
/path/to/your/sourceDebianRules 是此项目的 debian 构建目录，帮我转换为玲珑构建配置文件 linglong.yaml

https://linux.apps.demo.com/download/demo.orig.tar.xz 是一个开源项目源码包，
帮我转换为玲珑构建配置文件 linglong.yaml
```

### linyaps-app-packaging-script-generator

- **仓库地址**：<https://github.com/OpenAtom-Linyaps/linyaps-app-packaging-script-generator>

#### 简介

用于批量将 Debian 软件包（.deb）、tar 二进制归档包（.tar.zst 等）和 AppImage 应用转换为玲珑（Linyaps）便捷打包脚本工程目录的 Agent。

#### 建议使用场景

- 需要批量将 deb / tar 归档 / AppImage 转换为玲珑工程
- 手动编写 `linglong.yaml` 存在客观门槛
- 现有转制工具不支持的非标准格式包，需 AI 辅助处理

#### skills 能力介绍

| Skill 名称 | 功能描述 |
|------------|----------|
| `appimage-linyaps` | 将 AppImage 应用程序转换为玲珑包格式，基于 `tar-linyaps` 技能架构，针对 AppImage 特性进行优化 |
| `tar-linyaps` | 将 Linux binary release tar 归档包转换成玲珑应用便捷打包脚本 |
| `compat-testing` | 执行玲珑打包构建测试，验证生成工程能否正常构建，并运行兼容性检测确保应用在玲珑环境中正常运行 |
| `deb-analysis` | 解析 Debian 软件包（.deb）文件，提取元数据信息并解压文件内容，为后续玲珑打包工程生成提供基础数据 |
| `linglong-fix` | 根据验证报告自动修复玲珑构建项目中的问题，包括 `linglong.yaml` 格式、desktop 文件、图标目录结构、二进制文件权限等 |
| `linglong-project-gen` | 根据 deb 包信息和 CSV 配置，生成完整的玲珑打包工程，包括 `linglong.yaml` 配置文件和 `pak_linyaps.sh` 打包脚本 |
| `project-structure-validator` | 验证玲珑打包项目目录结构和必要文件的完整性，确保项目符合打包要求 |
| `resource-collector` | 从 deb 包解压后的目录中提取应用资源文件（desktop 文件、图标、appdata、补全脚本等），按照玲珑打包规范整理到 `files_res/` 目录结构中 |

#### 输出资源目录结构

支持手动修改、重复使用的应用打包脚本工程目录 `CI_${ll_id}`，`${ll_id}` 是实际项目对应的应用包名：

```text
CI_ll_app.netlify.ytdn
├── config
│   └── base_runtime_whitelist.conf
├── pak_linyaps.sh
├── reports
│   ├── structure_validation.json
│   └── yaml_validation.json
├── scripts
│   ├── dedup_desktop_files.sh
│   ├── handle_special_paths.sh
│   └── validate_bin_nesting.sh
└── templates
    ├── files_res
    │   └── share
    └── linglong.yaml
```

#### 打包工程使用示例

```bash
./pak_linyaps.sh \
  --linyaps_arch=x86_64 \
  --origin_version="3.6.5" \
  --src_path="/media/deepin/Data/top100-CI/260602-init/src/siyuan-3.6.5-linux.deb" \
  --output_dir="/media/deepin/Data/top100-CI/260602-init/output" \
  --build_tmp_dir="/home/deepin/.cache/siyuan"
```

参数说明：

| 参数 | 说明 |
|------|------|
| `--linyaps_arch` | 玲珑构建工程架构，如 `x86_64`、`arm64`、`loong64` |
| `--origin_version` | 源码上游版本号 |
| `--src_path` | 源码本地绝对路径 |
| `--output_dir` | layer 包输出目录 |
| `--build_tmp_dir` | 构建工程临时目录 |

> 部分 LLM 生成脚本时可能会自行去除参数，使用参数前需先确认当前 `pak_linyaps.sh` 支持该参数。

#### 建议提示词

```text
帮我把本地目录 /path/to/your/file 安装包转换为玲珑应用

https://linux.apps.demo.com/download/demo.deb 是一个 Linux 应用，帮我转换为玲珑应用
```

### linyaps-packaging-scripts-running-skill

- **仓库地址**：<https://github.com/OpenAtom-Linyaps/linyaps-packaging-scripts-running-skill>

#### 简介

打包执行 Agent，在已经持有可重复使用的构建工程模板后，接受用户传递的同一个应用的新版本来源（包、仓库），根据用户指定的构建工程模板所在目录进行打包。

#### 建议使用场景

- 已有打包工程模板，需定期更新版本（新 deb / 新源码）
- 需要将前两个 Agent 产出的工程模板落地为实际 layer 构建
- 日常打包维护中使用已有工程模板执行构建

#### skills 能力介绍

| Skill 名称 | 功能描述 |
|------------|----------|
| `linglong-binary-runner` | 通过 `pak_linyaps.sh` 执行玲珑二进制打包。用于已适配便捷打包脚本的项目（特征：目录下有 `pak_linyaps.sh`） |
| `linglong-source-updater` | 更新已初始化的 `linglong.yaml`，补充上游源码信息，更新构建规则，并打包为玲珑 layer。用于 source 类型任务（特征：目录下有 `linglong.yaml` 但缺少 sources 段）。支持 archive / git / file / dsc 四种源码类型 |

#### 建议提示词

```text
使用此软件包 https://linux.apps.demo.com/download/demo.deb 更新玲珑应用，
已经适配的工程目录在 /path/to/your/pak_linyaps.sh

使用此项目源码 https://linux.apps.demo.com/download/demo.orig.tar.xz 更新玲珑应用，
已经适配的工程目录在 /path/to/your/linglong.yaml
```
