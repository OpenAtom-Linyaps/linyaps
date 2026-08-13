<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# Building Linglong Apps with CI/CD

This chapter explains how to automate the build of Linglong apps in two ways: with GitHub Actions and with Docker.

## Building with GitHub Actions

[linglong-builder-action](https://github.com/myml/linglong-builder-action) is a GitHub Action that automatically installs `ll-builder` and performs the build and export inside a GitHub Actions workflow.

### Prerequisites

- The repository contains a `linglong.yaml` build configuration file
- Use an `ubuntu-24.04` runner

### Input Parameters

| Parameter | Description | Default |
| --- | --- | --- |
| `file` | Path to the `linglong.yaml` file | `./linglong.yaml` |
| `format` | Export format, either `layer` or `uab` | `layer` |

### Example Usage

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

### Workflow Notes

- `actions/checkout@v4`: checks out the current repository; use the `repository` parameter to check out a different repository instead
- `myml/linglong-builder-action@main`: installs `ll-builder`, enables the mirror cache and runs `ll-builder build`, then exports the artifact in `layer` or `uab` format according to the `format` parameter
- `actions/upload-artifact@v4`: uploads the build artifacts as a GitHub Artifact for easy download

## Building with Docker

[linglong-builder-docker](https://github.com/myml/linglong-builder-docker) provides a Docker image with `ll-builder` preinstalled, supporting the `amd64`, `arm64` and `loong64` architectures.

### Prerequisites

- Install [Docker](https://docs.docker.com/engine/install/) or [Podman](https://podman.io/docs/installation)

### Usage

```bash
docker run -ti --privileged \
  -v ll-builder-cache:/home/builder \
  registry.cn-hangzhou.aliyuncs.com/linyaps/builder:1.13.8_amd64
```

Once inside the container, you can use the `ll-builder` command to build apps.

### Parameter Notes

- `--privileged`: the container needs privileged mode to mount file systems
- `-v ll-builder-cache:/home/builder`: persists the build cache on the host to avoid repeated downloads

### Image List

| Image | Architecture | ll-builder Version |
| --- | --- | --- |
| `registry.cn-hangzhou.aliyuncs.com/linyaps/builder:1.13.8_amd64` | amd64 | 1.13.8 |
| `registry.cn-hangzhou.aliyuncs.com/linyaps/builder:1.13.8_arm64` | arm64 | 1.13.8 |
| `registry.cn-hangzhou.aliyuncs.com/linyaps/builder:1.13.8_loong64` | loong64 | 1.13.8 |
| `registry.cn-hangzhou.aliyuncs.com/linyaps/builder:1.10.3_amd64` | amd64 | 1.10.3 |
| `registry.cn-hangzhou.aliyuncs.com/linyaps/builder:1.10.3_arm64` | arm64 | 1.10.3 |
| `registry.cn-hangzhou.aliyuncs.com/linyaps/builder:1.10.3_loong64` | loong64 | 1.10.3 |

## Summary

- **GitHub Actions** is suitable for automated builds in a CI/CD workflow. It supports the `layer` and `uab` export formats and can leverage GitHub Cache to speed up builds.
- **Docker** is suitable for manual builds on a local machine or server. It supports multiple architectures and persists the cache through volume mounts.

Both approaches use the `ll-builder` command under the hood, and the build configuration is controlled by the `linglong.yaml` file in the repository.
