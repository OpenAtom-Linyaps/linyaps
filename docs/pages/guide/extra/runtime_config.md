<!--
SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# ll-cli 运行时配置

`ll-cli` 运行时配置用于调整应用运行行为或所在容器环境的配置，包括环境变量、挂载点、设备、扩展和桌面集成等。该功能面向熟悉容器与主机安全边界的高级用户和系统管理员。

运行时配置可以把主机文件、目录或设备暴露给容器环境。写入配置前应确认来源可信，并只授予应用必需的访问范围。配置变更只对之后运行的程序生效，测试变更前应先退出已有应用实例。

## 配置位置

`ll-cli run <appid>` 会读取系统级和用户级配置。标准安装中的路径如下：

| 级别 | 全局配置目录 | 单个应用配置目录 |
| --- | --- | --- |
| 系统级 | `/etc/linglong/` | `/etc/linglong/apps/<appid>/` |
| 用户级 | `$XDG_CONFIG_HOME/linglong/` | `$XDG_CONFIG_HOME/linglong/apps/<appid>/` |

如果未设置或设置了空的 `XDG_CONFIG_HOME`，用户级目录为 `$HOME/.config/linglong/`。

每个目录支持一个主配置文件和一组 drop-in 文件：

```text
linglong/
├── config.json
├── config.d/
│   ├── 10-device.json
│   └── 50-local.json
└── apps/
    └── org.example.demo/
        ├── config.json
        └── config.d/
            ├── 20-mount.json
            └── 90-user.json
```

主配置文件不是必需的，可以只创建 `config.d/`。drop-in 必须是 `config.d` 的直接子文件，并以 `.json` 结尾，其他文件会被忽略，文件名按字典序排序。

## 加载和生效顺序

运行应用时，`ll-cli` 按以下顺序加载配置，越靠后的配置优先级越高：

| 顺序 | 配置 |
| --- | --- |
| 1 | 系统级全局 `config.json` |
| 2 | 系统级全局 `config.d/*.json`，按文件名升序 |
| 3 | 系统级应用 `apps/<appid>/config.json` |
| 4 | 系统级应用 `apps/<appid>/config.d/*.json`，按文件名升序 |
| 5 | 用户级全局 `config.json` |
| 6 | 用户级全局 `config.d/*.json`，按文件名升序 |
| 7 | 用户级应用 `apps/<appid>/config.json` |
| 8 | 用户级应用 `apps/<appid>/config.d/*.json`，按文件名升序 |
| 9 | `--instance <name>` 选中的实例配置 |

应用目录中的 `<appid>` 应与 `ll-cli run` 使用的应用 ID 一致，例如 `org.deepin.calculator`。

任意一个已发现的 JSON 文件无法读取、语法错误、字段类型错误或包含无效枚举值时，本次运行会失败，而不是跳过该文件。配置文件修改后可以使用 `jq empty <file>` 先检查 JSON 语法。

系统级配置先于用户级配置加载。运行时配置适合提供默认值和本机定制，不应被当作限制用户权限的强制安全策略。每个字段的合并、覆盖和命令行优先级在下文对应字段中说明。

## 完整配置示例

配置文件必须是 JSON。所有顶层字段都是可选的。以下示例展示当前支持的全部字段：

```json
{
  "env": {
    "EXAMPLE_MODE": "desktop",
    "HTTP_PROXY": "http://127.0.0.1:7890"
  },
  "disable_xdp": false,
  "enable_pipewire": true,
  "device_mode": [
    "passthru"
  ],
  "devices": [
    "vendor.example/device=gpu0"
  ],
  "mounts": [
    {
      "source": "/srv/example-data",
      "destination": "/data",
      "type": "bind",
      "options": [
        "rbind",
        "ro"
      ],
      "src_type": "dir"
    }
  ],
  "ext_defs": {
    "org.deepin.runtime.dtk/23.1.0": [
      {
        "name": "org.example.runtime.extension",
        "version": "23.1.0",
        "directory": "/opt/extensions/org.example.runtime.extension",
        "allow_env": {
          "LD_LIBRARY_PATH": ""
        }
      }
    ]
  },
  "instances": {
    "development": {
      "env": {
        "EXAMPLE_DEBUG": "1"
      },
      "mounts": [
        {
          "source": "/home/user/example-source",
          "destination": "/workspace",
          "type": "bind",
          "options": [
            "rbind",
            "rw"
          ],
          "src_type": "dir"
        }
      ]
    }
  }
}
```

这个示例仅用于展示格式。主机路径、设备标识、扩展 ID 和版本必须替换为本机实际存在的值。JSON 字符串不会进行 shell 展开，因此不要在路径中使用 `~`、`$HOME` 或 `${HOME}`。

## 字段说明

### `env`

类型：对象，值必须是字符串。

为容器进程设置环境变量。不同名称的变量会合并，后加载配置中的同名键覆盖先加载配置；`ll-cli run --env KEY=VALUE` 又会覆盖配置文件中的同名键。

空对象 `{}` 不会删除已经定义的变量。要撤销变量，应修改或移除定义它的较早配置文件，或者在后续配置中将其设为所需的新字符串值。

```json
{
  "env": {
    "LANG": "zh_CN.UTF-8",
    "EXAMPLE_FEATURE": "enabled"
  }
}
```

### `disable_xdp`

类型：布尔值。

控制沙箱中的 XDG Desktop Portal 集成：

- `false`：尝试启用 Portal 集成。
- `true`：禁用 Portal 集成。

后加载配置中明确设置的值覆盖先加载配置；省略该字段不会改变已有值。命令行的 `--enable-xdp` 或 `--disable-xdp` 优先于所有配置文件。如果应用 ID 不符合 Portal ID 规则，`ll-cli` 可能自动禁用集成，除非显式使用 `--enable-xdp`。

### `enable_pipewire`

类型：布尔值。

设为 `true` 时，将当前用户的 PipeWire socket 挂载到容器，以便应用使用音视频服务；设为 `false` 时不启用该额外挂载。

后加载配置中明确设置的值覆盖先加载配置；省略该字段不会改变已有值。命令行 `--enable-pipewire` 最后应用，可以覆盖配置并显式启用；当前命令行没有对应的显式禁用参数。

### `device_mode`

类型：字符串数组。

设置设备访问模式。当前唯一支持的值是：

- `"passthru"`：把主机设备目录传递给容器，而不是只挂载默认设备节点。

多个配置文件中的值按加载顺序追加且不去重。空列表 `[]` 不会清除之前的值；要撤销透传模式，应修改或移除加入 `"passthru"` 的较早配置。

### `devices`

类型：字符串数组。

选择通过 CDI（Container Device Interface）加入容器的设备。每项格式为：

```text
<kind>=<name>
```

例如：

```json
{
  "devices": [
    "nvidia.com/gpu=gpu0"
  ]
}
```

设备必须存在于 `ll-cli` 能发现的 CDI 规范中，否则运行会失败。多个配置文件中的列表按加载顺序追加且不去重，空列表 `[]` 不会清除之前加入的设备；要撤销设备，应修改或移除定义它的较早配置。如果命令行提供了 `--device`，则命令行设备列表整体优先，不再使用配置文件中的列表。

### `mounts`

类型：挂载对象数组。

每个挂载对象支持：

| 字段 | 必需 | 类型 | 说明 |
| --- | --- | --- | --- |
| `source` | 是 | 字符串 | 主机上的源路径 |
| `destination` | 是 | 字符串 | 容器内的目标路径，推荐使用绝对路径 |
| `type` | 是 | 字符串 | OCI 挂载类型 |
| `options` | 否 | 字符串数组 | OCI 挂载选项，例如 `"bind"`、`"rbind"`、`"ro"`、`"rw"` |

多个配置文件中的挂载按加载顺序追加。空列表 `[]` 不会清除之前的挂载，同一 `destination` 的挂载也不会在合并阶段相互覆盖或去重；要撤销挂载，应修改或移除定义它的较早配置。当前没有直接添加这些持久挂载的 `ll-cli run` 命令行参数。

### `ext_defs`

类型：对象。键是需要扩展的 Base、Runtime 或应用引用，值是扩展定义数组。

目标键可以是应用 ID，也可以带版本，例如 `org.deepin.runtime.dtk/23.1.0`。只有与实际运行组件 ID 和版本匹配的定义才会参与扩展解析。

每个扩展定义支持：

| 字段 | 必需 | 类型 | 说明 |
| --- | --- | --- | --- |
| `name` | 是 | 字符串 | 已安装扩展包的 ID |
| `version` | 是 | 字符串 | 要解析的扩展版本；空字符串表示不限定版本 |
| `directory` | 是 | 字符串 | 扩展在容器中的挂载目录 |
| `allow_env` | 否 | 字符串对象 | 允许扩展修改的环境变量及其默认原值 |

未设置 `allow_env` 时，扩展声明的所有环境变量都可以生效。设置后，它同时充当白名单：未列出的变量会被忽略。映射值在原环境变量为空时作为扩展替换 `$ORIGIN` 的默认值。

不同目标键会合并。同一目标键在多个配置中出现时，其扩展定义按加载顺序追加，而不是按 `name` 覆盖或去重；空数组 `[]` 不会清除之前加入的定义。要撤销定义，应修改或移除较早配置。命令行 `--extensions` 指定的扩展会在运行时额外加入，不会删除 `ext_defs` 中匹配的定义。未安装的可选扩展会被跳过。

### `instances`

类型：对象。键是实例名称，值仍是一个运行时配置对象，支持本页列出的全部顶层字段。

不同实例名称会合并。同名实例分散在多个主配置和 drop-in 中时，其内部字段按照各字段自己的规则递归合并。空对象 `{}` 不会清除较早定义的实例。

实例用于为同一应用准备不同运行方案：

```json
{
  "env": {
    "MODE": "normal"
  },
  "instances": {
    "development": {
      "env": {
        "MODE": "debug"
      },
      "enable_pipewire": true
    }
  }
}
```

普通运行使用 `"MODE": "normal"`：

```bash
ll-cli run org.example.demo
```

指定 `--instance <name>` 时，会先完成系统级与用户级配置合并，再把对应实例的配置合并到结果末尾。因此实例中的标量和同名环境变量具有最高的配置文件优先级，列表则继续追加。选择实例后，示例中的 `MODE` 变为 `debug`：

```bash
ll-cli run --instance development org.example.demo
```

## 最小示例

只为一个应用增加只读目录挂载时，可创建：

```bash
mkdir -p "${XDG_CONFIG_HOME:-$HOME/.config}/linglong/apps/org.example.demo/config.d"
```

然后写入 `50-data.json`：

```json
{
  "mounts": [
    {
      "source": "/srv/example-data",
      "destination": "/data",
      "type": "bind",
      "options": [
        "rbind",
        "ro"
      ]
    }
  ]
}
```

退出已有实例后重新运行应用：

```bash
ll-cli run org.example.demo
```
