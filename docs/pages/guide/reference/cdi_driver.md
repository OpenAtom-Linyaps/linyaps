<!--
SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# 显卡驱动 CDI 适配指南

本文介绍 CDI 的基本概念、玲珑中的设备接入流程，以及驱动适配方如何编写配置、生成规范并验证应用中的设备访问。

## 背景

应用使用显卡时，通常需要访问宿主机上的三类资源：

- **设备节点**：例如 `/dev/dri/renderD128`，应用通过它与内核驱动交互。
- **用户态驱动文件**：例如图形接口实现库和厂商配置文件。
- **环境变量**：用于指定驱动实现或库的查找路径。

玲珑应用运行在容器中，容器内的文件系统与宿主机隔离。宿主机安装了驱动，并不意味着应用容器已经具备访问这些资源的条件。

CDI（Container Device Interface，容器设备接口）使用 JSON 或 YAML 文件描述设备及其所需的容器配置。容器运行时读取描述，将设备节点、挂载和环境变量加入容器。CDI 规范本身不安装驱动，设备访问权限仍由宿主机权限和容器内进程的身份决定。标准定义见 [CDI 规范](https://github.com/cncf-tags/container-device-interface/blob/main/SPEC.md)。

玲珑的 `ll-cdi-drv` 根据驱动适配配置生成 CDI 规范和默认设备选择，`ll-cli` 在启动应用时读取这些文件并注入设备。驱动也可以采用[扩展方式](./driver.md)接入。

## 原理

### 设备如何被描述和引用

一份 CDI 规范用 `kind` 标识设备类别，例如 `nvidia.com/gpu`。规范中的 `devices` 列表为每个设备集合指定一个 `name`。应用运行时使用完整名称引用设备：

```text
<kind>=<name>

nvidia.com/gpu=all
vendor.example/gpu=render
```

这里的“设备”是一个可以选择的资源集合，可以包含多个设备节点、挂载和环境变量，不要求与一块物理显卡一一对应。

### 从驱动配置到应用容器

```text
驱动适配方提供生成源配置
/etc/linglong/cdi-drv/<name>.json
                │
                ▼
          ll-cdi-drv
           ├── CDI 规范：描述设备资源
           │   /run/linglong-cdi-drv/spec/<name>.json 或 .yaml
           └── 设备启用配置：选择默认设备并绑定规范文件
               /run/linglong-cdi-drv/cfg/<name>.json
                │
                ▼
            ll-cli run
                │
                ▼
       应用容器中的设备节点、挂载和环境变量
```

三类配置文件各有用途：

| 文件 | 内容 | 维护方 |
| --- | --- | --- |
| 生成源配置 | 生成器、设备匹配规则、驱动路径和文件依赖 | 驱动适配方或管理员 |
| CDI 规范 | 设备类别、设备名和容器编辑项 | `ll-cdi-drv` 或其调用的生成工具 |
| 设备启用配置 | 应用默认使用的设备引用及对应规范路径 | `ll-cdi-drv` |

生成源配置的 `version: "1"` 表示玲珑配置格式版本。CDI 文件的 `cdiVersion` 表示 CDI 规范版本。内置生成器输出 `cdiVersion: "0.6.0"`。

`ll-cdi-drv` 提供两类生成器：

- `command`：调用厂商工具，读取其标准输出中的 CDI 规范。
- `linglong`：根据 DRM 匹配规则、设备路径、挂载和环境变量生成规范。

生成源配置和运行时目录由管理员维护。标准运行时目录由 root 写入，普通用户可读取；`/run` 中的产物在开机时重新生成。

## 示例

### CDI 规范中的设备

下面是一份说明 CDI 结构的规范。设备节点路径需要与宿主机上的实际设备一致：

```yaml
cdiVersion: 0.6.0
kind: vendor.example/gpu
devices:
  - name: render
    containerEdits:
      deviceNodes:
        - path: /dev/dri/renderD128
      env:
        - VENDOR_GPU_RUNTIME=1
```

`vendor.example/gpu=render` 表示选择该文件的 `render` 设备。其 `containerEdits` 指定加入容器的设备节点和环境变量；实际驱动需要的库和配置文件也可以通过 CDI 的 `mounts` 描述。

### 调用 NVIDIA 工具生成规范

将以下内容保存为 `/etc/linglong/cdi-drv/nvidia.json`。宿主机需要提供 `/usr/bin/nvidia-ctk` 及相应的驱动：

```json
{
  "version": "1",
  "generator": "command",
  "autoEnable": true,
  "defaultDevice": "all",
  "argv": ["/usr/bin/nvidia-ctk", "cdi", "generate"],
  "watchPathChanged": [
    "/usr/bin/nvidia-ctk",
    "/lib/modules/%v/modules.dep",
    "/lib/modules/%v/modules.dep.bin"
  ]
}
```

此配置执行 `nvidia-ctk cdi generate`，选择工具输出中的 `all` 设备作为默认设备。`watchPathChanged` 声明生成工具和模块依赖文件变化时需要刷新这个配置。

### 使用内置生成器描述驱动资源

下面是 `/etc/linglong/cdi-drv/vendor.json` 的示例。它使用虚构厂商的驱动名和路径，适配时需要替换为真实值：

```json
{
  "version": "1",
  "generator": "linglong",
  "kind": "vendor.example/gpu",
  "profile": {
    "drm": {
      "kernelDriver": "vendor_driver"
    },
    "deviceNodes": [
      { "path": "/dev/vendor-control" }
    ],
    "mountFiles": [
      "/usr/share/glvnd/egl_vendor.d/10_vendor.json",
      "${LIB_DIR}/libEGL_vendor.so.0"
    ],
    "env": ["VENDOR_GPU_RUNTIME=1"]
  }
}
```

此示例选择 `vendor_driver` 的 DRM 结点，并加入设备节点、驱动文件和环境变量。省略 `drm.nodes` 时，默认选择 `primary` 和 `render` 两类结点。未指定 `deviceProfile` 和 `defaultDevice` 时，设备名由配置文件名生成，因此完整引用为 `vendor.example/gpu=vendor-linglong`；也可以用 `defaultDevice` 指定单个设备的名称。设备默认自动启用，无需填写 `autoEnable`；路径默认必需，无需填写 `required`。必需路径缺失时，该设备不参与发布。

### 生成的设备启用配置

当 NVIDIA 工具输出的类别为 `nvidia.com/gpu`、包含 `all` 设备时，对应的 `/run/linglong-cdi-drv/cfg/nvidia.json` 为：

```json
{
  "devices": [
    {
      "name": "nvidia.com/gpu=all",
      "spec": "../spec/nvidia.yaml"
    }
  ]
}
```

`name` 是完整设备引用，`spec` 指向与该启用配置一起发布的 CDI 文件。`autoEnable: true` 使该设备参与应用的默认选择；`autoEnable: false` 生成空的 `devices` 列表，设备仍可通过显式引用选择。

不同规范可以生成相同完整设备名 `<kind>=<name>` 的设备，所有规范和默认启用配置都会保留。运行时按完整设备名去重，只选择第一项及其绑定的规范，不合并其他同名设备的编辑。跳过同名引用时会警告，并列出保留与跳过的规范来源。自动启用项按生成的 `cfg` 文件名顺序选择；同名设备在不同规范中的资源不会同时应用。

## 使用方法

### 1. 准备驱动和源配置

确认宿主机已有所需的内核驱动、用户态库和设备节点。使用 `command` 生成器时，还需要安装 `argv` 中指定的工具。

将源配置安装到配置目录，例如：

```bash
sudo install -d -m 0755 /etc/linglong/cdi-drv
sudo install -m 0644 nvidia.json /etc/linglong/cdi-drv/nvidia.json
```

本文路径按标准系统安装描述。配置目录随构建和打包设置确定。`ll-cdi-drv` 安装在安装前缀的 `libexec/linglong/` 下；下文命令中的 `ll-cdi-drv` 在执行时应使用其实际可执行路径，或确保命令位于执行用户的 `PATH` 中。

### 2. 手动刷新与提交请求

```bash
jq empty /etc/linglong/cdi-drv/nvidia.json
sudo ll-cdi-drv refresh nvidia

cat /run/linglong-cdi-drv/spec/nvidia.yaml
cat /run/linglong-cdi-drv/cfg/nvidia.json
```

刷新使用的配置名是文件名去掉 `.json` 后的部分。手动命令的诊断信息输出到终端；需要结合日志和产物判断是否生成了所需设备。内置生成器输出 `<name>.json`；command 生成器保留工具的原始输出，根据内容使用 `<name>.json` 或 `<name>.yaml`。目录扫描只读取 `.json`、`.yaml`、`.yml` 文件，忽略其他后缀的备份和临时文件。显式指定文件时不限制后缀；读取内容时先尝试 JSON，解析失败后再尝试 YAML。

| 命令 | 行为 |
| --- | --- |
| `ll-cdi-drv refresh` | 立即刷新全部配置 |
| `ll-cdi-drv refresh nvidia` | 立即刷新 `nvidia.json` 对应的监听和产物 |
| `ll-cdi-drv refresh --request-all` | 提交全量请求 |
| `ll-cdi-drv refresh --request nvidia` | 提交单个配置的请求 |
| `ll-cdi-drv refresh-pending` | 持续消费请求并刷新，队列空闲达到指定时长后退出，默认 5 秒 |

这些管理命令通常以 root 身份执行。提交请求只登记需要刷新的配置，不立即生成产物；可手动执行 `refresh-pending` 处理请求，或由下文的自动刷新服务处理。

消费期间到达的新请求由同一进程继续处理，每次刷新完成后重新计算空闲时间。等待新请求时不持有刷新锁，手动刷新仍可执行。消费失败时返回错误，已领取的请求不会重新入队。

`refresh-pending` 支持 `--idle-timeout` 指定非负整数秒数，默认值为 `5`。设为 `0` 时，处理到队列为空便立即退出。例如，`ll-cdi-drv refresh-pending --idle-timeout 10` 会等待队列连续空闲 10 秒后退出。

立即刷新和消费命令接受以下选项：

| 选项 | 用途 |
| --- | --- |
| `--config-dir` | 指定源配置目录 |
| `--runtime-dir` | 指定产物和请求队列目录 |
| `--unit-dir` | 指定 systemd 运行时单元目录 |
| `--systemctl` | 指定 systemctl 可执行程序 |

提交请求时不能指定 `--config-dir`。配置名必须非空，只能包含 ASCII 字母、数字和 `._-`，且不能以点开头。

### 3. 在应用中验证设备

默认设备启用后，运行实际安装的应用即可使用设备选择：

```bash
ll-cli run org.example.demo
```

也可以显式绑定规范文件和设备：

```bash
ll-cli run --cdi-spec /run/linglong-cdi-drv/spec/nvidia.yaml \
  --device nvidia.com/gpu=all org.example.demo
ll-cli run org.example.demo -- ls /dev/dri
```

将 `org.example.demo` 替换为待验证应用的 ID。配置更新作用于之后创建的容器，验证前应退出已有应用实例。

### 4. 自动刷新规则与启用方法

自动刷新由 systemd 管理，触发规则如下：

| 触发条件 | 刷新范围 |
| --- | --- |
| 系统开机 | 全部源配置 |
| `/etc/linglong/cdi-drv/` 中的源配置新增、修改或删除 | 全部源配置 |
| 某个配置的 `watchPathChanged` 依赖变化 | 仅该配置 |
| 提交刷新请求 | 请求指定的全部或单个配置 |

启用开机刷新、配置目录监听和请求自动处理：

```bash
sudo systemctl daemon-reload
sudo systemctl enable --now ll-cdi-drv-config.path ll-cdi-drv-refresh.path ll-cdi-drv-config.service
```

`watchPathChanged` 对应的监听在配置刷新时同步并启动，无需逐个手动启用。设备新增、移除或更换后，需要管理员手动刷新，或由外部事件源提交请求。

项目的 `91-linglong.preset` 声明了这三个入口的默认启用策略。安装脚本可以应用该策略；管理员也可以使用上面的命令显式启用。仅安装 preset 文件不会立即启用或启动单元。

### 5. 更新、移除和排查

卸载驱动时删除其源配置，刷新会清理对应的监听和产物。需要立即清理时，在删除源配置后执行：

```bash
sudo ll-cdi-drv refresh nvidia
```

普通运行时配置中手工声明的设备引用也需要由维护方同步更新。

通过 systemd 执行的刷新可以这样排查：

```bash
journalctl -u ll-cdi-drv-config.service -u ll-cdi-drv-refresh.service
systemctl status ll-cdi-drv-config.path ll-cdi-drv-refresh.path
systemctl status ll-cdi-drv-watch@nvidia.path
journalctl -u ll-cdi-drv-request@nvidia.service
```

产物缺失时，检查生成器标准输出、必需路径和 DRM 匹配条件。修复错误后重新执行刷新或提交请求，失败请求不会自动重试。手动刷新正常而文件变化未触发刷新时，检查固定入口的状态和动态 `.path` 中的监听路径。

## 规范

源配置字段定义见源码中的 `libs/cdi_drv/schema.json`。本节描述配置含义和运行时选择规则。

### 源配置字段

隐藏文件和配置名不合法的文件不参与配置枚举。每个合法的 `.json` 文件使用独立的配置名；源配置读取普通文件，目录和符号链接会被跳过。

| 字段 | 类型和含义 |
| --- | --- |
| `version` | 必填字符串，值为 `"1"` |
| `generator` | 必填，`command` 或 `linglong` |
| `autoEnable` | 布尔值，默认 `true`，控制是否写入该配置的默认设备引用 |
| `defaultDevice` | 默认启用的最终设备名；内置生成器没有设备配置时，用于命名单个设备。省略时的行为见下文 |
| `watchPathChanged` | 驱动文件、库和生成工具等依赖的绝对路径数组 |
| `argv` | `command` 生成器的程序和参数数组，首项为非空程序名 |
| `timeout` | `command` 生成器的执行超时，单位为秒，整数，默认 `30`；小于等于 `0` 表示不限制执行时间 |
| `kind` | `linglong` 生成器的设备类别，格式为 `<vendor>/<class>` |
| `profile` | `linglong` 生成器的公共资源配置 |
| `deviceProfile` | `linglong` 生成器的设备配置数组，每项包含 `device` 和 `profile` |

### command 的输入输出

`argv` 直接执行程序，不经过 shell。程序名可以是绝对路径，也可以通过可执行程序搜索找到。标准输出提供一份有效的 CDI 规范，类别和设备名取自该规范；标准错误用于诊断信息。

`timeout` 限制单次命令执行及输出读取的总时长，默认 30 秒。例如，`"timeout": 60` 将限制设为 60 秒，`"timeout": 0` 或负数表示不限制执行时间。超时后终止生成器进程组并回收直接子进程，该源配置按生成失败处理，继续处理其他源配置，已领取的请求不会重新入队。此超时与 `refresh-pending --idle-timeout` 的队列空闲等待时间相互独立。

输出可以是 YAML 或 JSON。YAML 读取使用第一份文档。`defaultDevice: "all"` 表示选择名为 `all` 的设备，`all` 的资源范围由生成工具输出决定。

### 内置生成器的资源规则

内置生成器需要 `kind`，以及 `profile` 或非空的 `deviceProfile`。未提供 `profile`，且 `deviceProfile` 缺失或为空时，报告缺少资源配置并跳过该配置。

- 顶层 `profile` 对各设备提供公共资源配置。
- `deviceProfile` 每项生成一个 CDI device，`device` 是最终设备名。
- 未提供设备配置或数组为空时，公共配置生成一个设备，名称取 `defaultDevice`；省略时使用 `<配置名>-linglong`，例如 `vendor-linglong`。设备名必须符合 CDI 设备名规则。
- `deviceNodes`、`mountFiles` 和 `env` 按公共配置在前、设备配置在后的顺序追加。
- 设备级 `drm` 整体替换公共 `drm`；省略时继承公共值。
- 设备配置数组非空时，`defaultDevice` 必须指向数组中声明的设备，不会额外生成一个默认设备；省略时选择第一个实际生成的设备。选中的设备生成失败时，不写入其默认启用引用。

`drm` 使用对象表示匹配规则：

| 字段 | 规则 |
| --- | --- |
| `kernelDriver` | 可省略；提供时按内核驱动名过滤 |
| `pciIds` | 可省略或为空，表示不限制；支持 `1234:5678` 和 `1234:*`，多个条目为候选匹配项 |
| `nodes` | `render`、`primary` 的数组；省略时选择两类，空数组表示跳过 DRM 结点选择 |

驱动名与 PCI 条件同时提供时，两者都需要满足。多个匹配结点可以属于同一个 CDI device。

设备路径和挂载路径规则：

| 配置项 | 宿主机路径 | 容器路径 |
| --- | --- | --- |
| `deviceNodes` 对象 | `hostPath`；省略时使用 `path` | `path` |
| `mountFiles` 字符串 | 字符串经过变量替换后的路径 | 与宿主机路径相同 |
| `mountFiles` 对象 | `hostPath` 经过变量替换后的路径 | `path`；省略时与宿主机路径相同 |

挂载使用 `ro,nosuid,nodev,rbind,rprivate`。源路径可以是文件、目录等已存在路径。`required` 默认为 `true`；必需路径缺失时跳过对应设备，`required: false` 时只跳过缺失项。`mountFiles` 字符串项按必需路径处理。

设备没有节点、挂载和环境变量时不生成产物。未匹配到 DRM 结点时，如果其他资源仍然存在，仍可生成设备。最终没有有效设备时，该配置不产生 CDI 和启用产物。

`${LIB_DIR}` 只在 `mountFiles` 的宿主机路径中替换，表示目标系统默认库目录。构建变量 `LINGLONG_CDI_DRV_LIB_DIR` 提供默认值，运行时同名非空环境变量可以覆盖。`$$` 表示字面量 `$`，所以 `$${LIB_DIR}` 得到字面量 `${LIB_DIR}`。

### 文件依赖监听规则

`watchPathChanged` 的条目对应 systemd `PathChanged=`，一个配置的任意依赖变化都提交该配置的刷新请求。目录监听不递归；子目录内的变化需要由相应文件或子目录条目覆盖。语义见 [systemd.path](https://www.freedesktop.org/software/systemd/man/latest/systemd.path.html)。

路径支持 systemd specifier：`%v` 表示运行中的内核版本，`%%` 表示字面量 `%`。路径不支持换行、回车、末尾空格或制表符，以及末尾反斜杠。示例中的监听项用于驱动文件和工具依赖；设备变化由外部事件源或手动刷新处理。

inotify 不能可靠反映 `/sys/module/nvidia/version` 等虚拟文件的内容变化，这类文件不能替代设备或内核模块事件源，见 [inotify(7)](https://man7.org/linux/man-pages/man7/inotify.7.html)。

### 运行时设备选择规则

`devices` 接受字符串和指定规范文件的对象：

```json
{
  "devices": [
    "vendor.example/gpu=render",
    {
      "name": "other.example/gpu=gpu0",
      "spec": "/etc/cdi/other-gpu.yaml"
    }
  ]
}
```

字符串使用 `<kind>=<name>`；对象的 `name` 为完整引用，`spec` 绑定对应规范。普通配置的位置和应用级规则见 [`ll-cli` 运行时配置](../extra/runtime_config.md)。生成的 `cfg` 文件只提供设备选择，其中仅读取 `devices`。

以下规则中的普通配置，是系统、用户、应用和实例配置按加载顺序合并后的结果。各配置文件的 `devices` 列表依次追加；空数组不会清除较早配置中的设备。

设备选择按以下规则进行：

1. 命令行出现 `--device` 时，使用这一组命令行设备；多个 `--device` 互相追加，同一完整设备名只取命令行中第一次出现的引用。
2. 没有命令行设备时，普通配置中非空的 `devices` 与自动启用项合并。普通配置覆盖所有相同完整设备名的自动项，并按合并后的配置列表顺序选择第一次出现的引用；其他完整设备名的自动项继续保留。
3. 合并后的普通配置显式包含 `"devices": []` 时，设备选择为空，也不使用自动启用项。若较早配置已添加设备，后续配置中的空数组不能使合并结果变空。
4. 普通配置没有 `devices` 时，使用生成的自动启用项。没有设备选择时，`ll-cli` 在普通规范目录搜索 `nvidia.com/gpu=all`。显式的空设备列表不会进入这一探测。

字符串设备引用依次搜索当前发布版本的 `spec`、`/etc/linglong/cdi`、`/etc/cdi`、`/var/run/cdi`。`--cdi-spec-dir` 指定普通规范搜索目录及其优先级。同一目录中的规范按文件名排序，选择第一个有效且匹配 `kind` 和 `name` 的规范；新增更高优先级的同名设备可能改变裸引用的选择结果。对象中的 `spec` 和命令行 `--cdi-spec` 直接绑定文件，不参与目录搜索；`--cdi-spec` 对之后出现的 `--device` 生效：

```bash
ll-cli run \
  --cdi-spec /etc/cdi/vendor-a.yaml --device vendor.example/gpu=gpu0 \
  --cdi-spec /etc/cdi/vendor-b.yaml --device vendor.example/gpu=gpu1 \
  org.example.demo
```

相对的 `--cdi-spec-dir` 按调用时的工作目录解释；读取后将规范路径转换为绝对路径再保存到运行上下文，避免恢复上下文时依赖原工作目录。

目录扫描遇到不可读或无效规范时告警并跳过。显式绑定文件读取失败或所选设备不存在时会报错。不同规范的设备编辑按最终设备列表顺序应用。
