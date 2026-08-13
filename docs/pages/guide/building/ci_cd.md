<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# 使用 CI/CD 构建玲珑应用

本章介绍如何通过 GitHub Actions 和 Docker 两种方式自动化构建玲珑应用。

## 使用 GitHub Actions 构建

[linglong-builder-action](https://github.com/myml/linglong-builder-action) 是一个 GitHub Action，可在 GitHub Actions 工作流中自动安装 `ll-builder` 并完成构建和导出。

### 前置条件

- 仓库中包含 `linglong.yaml` 构建配置文件
- 使用 `ubuntu-24.04` 运行器

### 输入参数

| 参数 | 说明 | 默认值 |
| --- | --- | --- |
| `file` | `linglong.yaml` 文件路径 | `./linglong.yaml` |
| `format` | 导出格式，可选 `layer` 或 `uab` | `layer` |

### 用法示例

```yaml
name: CI
on:
  push:
    branches: ["main"]
  pull_request:
    branches: ["main"]
  workflow_dispatch:

jobs:
  build:
    runs-on: ubuntu-24.04

    steps:
      - uses: actions/checkout@v4

      - name: ll-builder-build
        uses: myml/linglong-builder-action@main

      - uses: actions/upload-artifact@v4
        with:
          name: layers
          path: ./*.layer
```

### 工作流说明

- `actions/checkout@v4`：检出当前仓库代码，可以使用 `repository` 参数改为其他的仓库
- `myml/linglong-builder-action@main`：安装 `ll-builder`、启用镜像缓存并执行 `ll-builder build`，然后根据 `format` 参数导出 `layer` 或 `uab` 格式的产物
- `actions/upload-artifact@v4`：将构建产物上传为 GitHub Artifact，方便下载

## 使用 Docker 构建

[linglong-builder-docker](https://github.com/myml/linglong-builder-docker) 提供了预装 `ll-builder` 的 Docker 镜像，支持 `amd64`、`arm64` 和 `loong64` 三种架构。

### 前置条件

- 安装 [Docker](https://docs.docker.com/engine/install/) 或 [Podman](https://podman.io/docs/installation)

### 使用方法

```bash
docker run -ti --privileged \
  -v ll-builder-cache:/home/builder \
  registry.cn-hangzhou.aliyuncs.com/linyaps/builder:1.13.8_amd64
```

进入容器后即可使用 `ll-builder` 命令构建应用。

### 参数说明

- `--privileged`：容器需要特权模式以挂载文件系统
- `-v ll-builder-cache:/home/builder`：将构建缓存持久化到宿主机，避免重复下载

### 镜像列表

| 镜像 | 架构 | ll-builder 版本 |
| --- | --- | --- |
| `registry.cn-hangzhou.aliyuncs.com/linyaps/builder:1.13.8_amd64` | amd64 | 1.13.8 |
| `registry.cn-hangzhou.aliyuncs.com/linyaps/builder:1.13.8_arm64` | arm64 | 1.13.8 |
| `registry.cn-hangzhou.aliyuncs.com/linyaps/builder:1.13.8_loong64` | loong64 | 1.13.8 |
| `registry.cn-hangzhou.aliyuncs.com/linyaps/builder:1.10.3_amd64` | amd64 | 1.10.3 |
| `registry.cn-hangzhou.aliyuncs.com/linyaps/builder:1.10.3_arm64` | arm64 | 1.10.3 |
| `registry.cn-hangzhou.aliyuncs.com/linyaps/builder:1.10.3_loong64` | loong64 | 1.10.3 |

## 总结

- **GitHub Actions** 适合在 CI/CD 流程中自动构建，支持 `layer` 和 `uab` 两种导出格式，可利用 GitHub Cache 加速构建
- **Docker** 适合在本地或服务器上手动构建，支持多架构，通过卷挂载持久化缓存

两种方式底层均使用 `ll-builder` 命令，构建配置通过仓库中的 `linglong.yaml` 控制。