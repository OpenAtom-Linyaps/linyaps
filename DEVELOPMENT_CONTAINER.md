# OrbStack 开发环境

该环境使用 Ubuntu 24.04、Qt 6 和仓库 CI 相同的系统依赖，适用于 Apple Silicon 上的 OrbStack。

开发进程以容器内普通用户运行。容器启用 `privileged`，用于提供 `/dev/fuse`、Linux Namespace
等 Linyaps 沙箱和完整测试所需的内核能力；请只在可信的本地代码上使用此环境。

## 启动

```bash
orbctl start
docker compose up -d --build
```

## 配置、编译和测试

```bash
docker compose exec dev cmake -B build -GNinja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCPM_LOCAL_PACKAGES_ONLY=ON \
  -DLINGLONG_ENABLE_WAYLAND_SEC_CTX_SUPPORT=ON
docker compose exec dev cmake --build build --parallel 4
docker compose exec dev ctest --test-dir build --output-on-failure
```

`CPM_LOCAL_PACKAGES_ONLY=ON` 会使用镜像中安装的依赖，避免配置阶段临时从 GitHub 下载依赖。

## 进入容器

```bash
docker compose exec dev bash
```

构建产物位于宿主机的 `build/` 目录；ccache 缓存保存在已被 Git 忽略的 `.cache/ccache/` 中。

## 停止

```bash
docker compose down
```

## 已知限制

容器只编译本仓库，不安装 linyaps 的运行时数据，因此部分命令开箱不可用：

- OCI 运行时 `ll-box` 来自独立的 linyaps-box 项目，不在本仓库构建。
  `ll-builder build/run` 等需要沙箱的命令会报 `ll-box not found`；
  可以安装 linyaps-box，或临时指向其他 OCI 运行时（如 `crun`）：

  ```bash
  export LINGLONG_OCI_RUNTIME=crun
  ```

- 仓库相关命令（`ll-builder list/push` 等）会回退读取
  `$LINGLONG_DATA_DIR/config.yaml`（默认 `/usr/share/linglong/config.yaml`），
  容器内未安装该文件；从仓库复制一份即可：

  ```bash
  sudo cp misc/share/linglong/config.yaml /usr/share/linglong/config.yaml
  ```

`ll-cli doctor`（构建产物）会逐项检查上述组件并给出提示。
