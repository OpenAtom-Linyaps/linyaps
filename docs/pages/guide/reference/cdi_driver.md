<!--
SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# 显卡驱动 CDI 适配指南

如意玲珑应用的运行容器与宿主机环境隔离，显卡等硬件设备需要通过受控方式注入容器。除[驱动程序](./driver.md)介绍的扩展（Extension）方式外，驱动厂商还可以采用 CDI（Container Device Interface）方案：以标准化的描述文件声明设备及其注入细节，由玲珑在应用运行时按需加入容器。

适配 CDI 方案的整体流程：

1. 驱动包提供 CDI 规范生成工具，根据当前设备状态生成规范文件和玲珑配置。
2. 驱动包安装 systemd 服务，在开机时生成基线配置，并在驱动文件更新、配置生成工具更新和设备热插拔等时机按需重新生成。
3. 设备移除、更换或驱动卸载时，同步移除或重建相关配置，避免残留引用导致应用无法启动。

## CDI 标准简介

CDI（Container Device Interface，容器设备接口）是社区制定的容器设备接入标准，已被主流容器运行时广泛支持。规范详情见 [container-device-interface](https://github.com/cncf-tags/container-device-interface)。

CDI 的核心思想：

- 驱动厂商以 JSON 或 YAML 文件描述一类设备，包括设备节点、挂载和环境变量等注入细节。
- 规范文件存放在 `/etc/cdi` 和 `/var/run/cdi` 目录（`/var/run` 通常为 tmpfs，重启后内容丢失，持久化文件建议写入 `/etc/cdi`），文件名以 `.json`、`.yaml` 或 `.yml` 结尾。
- 每个规范文件通过 `kind` 声明设备类别，格式为 `<vendor>/<class>`，例如 `nvidia.com/gpu`，其中 `vendor` 需为合法的 DNS 域名。
- 规范中的设备通过完整名称引用，格式为 `<kind>=<设备名>`，例如 `nvidia.com/gpu=gpu0`。
- 容器运行时按规范注入设备，无需理解每种硬件的节点规则。

对显卡驱动而言，CDI 方案的优势：

- 驱动包只需维护描述文件，无需修改应用或玲珑本体。
- 设备热插拔、更换后只需更新描述文件，玲珑在下次运行应用时自动采用。
- 玲珑容器以非特权用户运行，设备节点的可见性与权限通过规范文件和宿主机 udev 规则统一控制。

## 玲珑配置中 devices 的启用规则

`ll-cli` 运行时配置中的 `devices` 字段用于选择通过 CDI 加入容器的设备。配置的完整说明见 [`ll-cli` 运行时配置](../extra/runtime_config.md)，适配驱动时需关注以下规则：

| 规则 | 说明 |
| --- | --- |
| 配置格式 | 字符串数组，每项为 `<vendor>/<class>=<设备名>`，例如 `vendor.example/gpu=gpu0` |
| 配置位置 | 系统级 `/etc/linglong/`，用户级 `$XDG_CONFIG_HOME/linglong/`；推荐写入系统级配置目录中驱动自己的 `config.d/*.json` |
| 设备校验 | 设备必须存在于 `ll-cli` 能发现的 CDI 规范中，否则应用运行会失败 |

需要特别注意：显式引用不存在设备的配置会导致应用运行失败。若将设备引用写入系统级全局配置（如 `/etc/linglong/config.d/`），则设备移除后所有应用的启动都会受影响。因此适配方必须保证配置与设备状态同步，下文的生成程序会在检测不到设备时移除相关配置。

## 生成示例：CDI 规范与玲珑配置

### 手工编写示例

假设厂商 PCI Vendor ID 为 `0x1234`，设备类别为 `vendor.example/gpu`。CDI 规范文件 `/etc/cdi/vendor-gpu.yaml`：

```yaml
cdiVersion: 0.6.0
kind: vendor.example/gpu
devices:
  - name: gpu0
    containerEdits:
      deviceNodes:
        - path: /dev/dri/card0
        - path: /dev/dri/renderD128
        - path: /dev/vendor/control
containerEdits:
  env:
    - VENDOR_GPU_RUNTIME=1
  mounts:
    - hostPath: /usr/lib/vendor-gpu
      containerPath: /usr/lib/vendor-gpu
      type: bind
      options: [ro]
```

主要字段说明：

| 字段 | 说明 |
| --- | --- |
| `cdiVersion` | CDI 规范版本，当前推荐 `0.6.0` |
| `kind` | 设备类别，格式 `<vendor>/<class>`，需与玲珑配置中的设备引用一致 |
| `devices[].name` | 设备名，与 `kind` 组成完整引用名，如 `vendor.example/gpu=gpu0` |
| `devices[].containerEdits.deviceNodes` | 该设备注入容器的设备节点，`path` 为宿主机节点路径 |
| `containerEdits` | 规范级编辑项，对该文件中所有设备生效，可声明 `env`、`deviceNodes`、`mounts` 等 |

对应的玲珑配置写入 `/etc/linglong/config.d/30-vendor-gpu.json`（系统级全局，对所有应用生效）：

```json
{
  "devices": [
    "vendor.example/gpu=gpu0"
  ]
}
```

### 自动生成

手工编写的文件无法感知设备变化。推荐在驱动包中提供配置生成程序（如 `/usr/bin/vendor-gpu-cdi-generate`），每次触发时按当前设备状态全量重建或移除两个文件。要点：

- 通过 `/sys/class/drm/*/device/vendor` 过滤**本厂商或者当前驱动支持**的 DRM 设备节点。
- 检测不到本厂商设备时，**移除 CDI 规范与玲珑配置，避免残留引用**。
- 保持玲珑配置、CDI 规范与当前设备、驱动状态一致。

生成程序伪代码：

```text
main:
    # 1. 枚举设备：扫描本驱动支持的硬件（如通过 /sys/class/drm/*/device/vendor
    #    过滤本厂商的 DRM 设备节点），得到设备节点列表
    nodes = 枚举本驱动支持的设备节点()

    # 2. 无设备时清理：移除 CDI 规范与玲珑配置并退出，
    #    避免玲珑配置残留对不存在设备的引用
    if nodes 为空:
        移除规范文件和玲珑配置文件
        return

    # 3. 生成 CDI 规范：按设备节点列表构建 devices 与 containerEdits，
    #    包括设备节点、环境变量、驱动库挂载等注入细节
    规范 = 构建 CDI 规范(kind, nodes, 环境变量, 挂载)

    # 4. 生成玲珑配置：devices 列表中每项为 <kind>=<设备名>，
    #    设备名与规范中的 devices[].name 一致
    玲珑配置 = 构建 devices 列表(规范中的设备名)
```

生成程序以 `0755` 权限安装，必须保证幂等：无论由何种事件触发，都按当前设备与驱动状态全量重建或移除文件。多卡、多节点等复杂拓扑建议在厂商工具中按 PCI 对应关系精确生成，伪代码仅演示通用流程。

### 按需触发

生成程序由 systemd 服务（oneshot 类型）执行，在开机时生成基线配置，并在各类变化时机按需重新运行：

```ini
# /usr/lib/systemd/system/vendor-gpu-cdi.service
[Unit]
Description=生成并维护厂商 GPU 的 CDI 规范与玲珑配置
After=systemd-modules-load.service local-fs.target

[Service]
Type=oneshot
ExecStart=/usr/bin/vendor-gpu-cdi-generate

[Install]
WantedBy=multi-user.target
```

#### 自动触发

当驱动文件变化、规范生成工具更新或设备发生变化时，需要重新运行生成程序。可以使用 systemd path 单元监听相应路径并自动重新运行服务，使 CDI 规范与玲珑配置始终与当前设备、驱动状态一致。一般可以监听的路径包括：

- **设备目录**（如 `/dev/dri`）：目录内容变化（设备节点的创建、删除）均会触发，覆盖设备热插拔、更换和数量变化。
- **驱动文件与生成工具**（如 `/usr/lib/vendor-gpu`、`/usr/bin/vendor-gpu-cdi-generate`）：覆盖驱动库或工具自身被升级、替换的情况。

```ini
# /usr/lib/systemd/system/vendor-gpu-cdi.path
[Unit]
Description=监听设备与驱动文件变化并刷新 CDI 规范

[Path]
PathChanged=/dev/dri
PathChanged=/usr/lib/vendor-gpu
PathChanged=/usr/bin/vendor-gpu-cdi-generate
Unit=vendor-gpu-cdi.service

[Install]
WantedBy=multi-user.target
```

#### 驱动包安装时触发

驱动文件、配置生成工具的更新通常发生在驱动包升级时。同一驱动包内，单元的启用与刷新统一由安装钩子完成，无需等待 path 监听发现文件变化：

```bash
# deb 的 postinst / rpm 的 %post
systemctl daemon-reload
systemctl enable --now vendor-gpu-cdi.path vendor-gpu-cdi.service
```

## 设备更改与卸载时的配置移除

### 设备热插拔与更换

设备被拔出、更换或数量减少时，path 单元触发服务，生成程序会：

- 设备全部消失：移除 `/etc/cdi/vendor-gpu.yaml` 与 `/etc/linglong/config.d/30-vendor-gpu.json`，应用不再引用不存在的设备。
- 设备更换或数量变化：全量重建规范与配置，保证 `gpu0`、`gpu1` 等引用始终与实际设备一致。

配置变更只对之后启动的应用生效，验证前应先退出已有应用实例。

### 驱动包卸载

驱动包的卸载钩子（deb 的 `prerm`/`postrm`，rpm 的 `%preun`/`%postun`）应停止并禁用按需触发的监听与服务，清理已生成的文件后重载单元定义：

```bash
systemctl disable --now vendor-gpu-cdi.path vendor-gpu-cdi.service
rm -f /etc/cdi/vendor-gpu.yaml /etc/linglong/config.d/30-vendor-gpu.json
systemctl daemon-reload
```

若规范文件写入了 `/var/run/cdi` 等临时目录，重启后自然消失；写入 `/etc/cdi` 的文件必须在卸载时显式移除。

## 验证与排查

排查整体思路：先确认生成程序输出了正确且与当前设备、驱动状态一致的文件，再验证玲珑能否发现并注入设备，最后排除残留配置的干扰。

### 前置检查

```bash
# 查看生成程序执行日志，确认最近一次触发时间与执行结果
journalctl -u vendor-gpu-cdi.service

# 查看 path 单元日志，确认监听是否正常、事件是否触发
journalctl -u vendor-gpu-cdi.path

# 检查玲珑配置 JSON 语法
jq empty /etc/linglong/config.d/30-vendor-gpu.json

# 确认 CDI 规范存在且设备引用与玲珑配置一致
cat /etc/cdi/vendor-gpu.yaml
cat /etc/linglong/config.d/30-vendor-gpu.json
```

### 验证设备注入

```bash
# 通过命令行一次性指定设备，验证规范是否有效；成功说明 CDI 侧无问题
ll-cli run --device vendor.example/gpu=gpu0 org.example.demo

# 在容器环境中确认设备节点已注入
ll-cli run org.example.demo -- ls /dev/dri

# 确认驱动库挂载与环境变量已进入容器环境
ll-cli run org.example.demo -- env | grep VENDOR_GPU_RUNTIME

# 进入容器环境排查
ll-cli run org.example.demo -- bash
```

### 常见问题

- CDI 配置格式错误、路径错误
- CDI 中描述的文件/设备不存在或发生变化
- 玲珑配置格式错误、路径错误
- 残留配置

