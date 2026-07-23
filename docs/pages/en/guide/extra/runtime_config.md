<!--
SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# ll-cli Runtime Configuration

The `ll-cli` runtime configuration adjusts application behavior and the container environment, including environment variables, mounts, devices, extensions, and desktop integration. It is intended for advanced users and system administrators who understand container and host security boundaries.

A runtime configuration can expose host files, directories, or devices to a container. Before writing one, verify that the source is trusted and grant only the access the application requires. Changes affect only programs launched afterward; exit existing application instances before testing a change.

## Configuration Locations

`ll-cli run <appid>` reads system-level and user-level configurations. A standard installation uses these paths:

| Level | Global configuration directory | Per-application configuration directory |
| --- | --- | --- |
| System | `/etc/linglong/` | `/etc/linglong/apps/<appid>/` |
| User | `$XDG_CONFIG_HOME/linglong/` | `$XDG_CONFIG_HOME/linglong/apps/<appid>/` |

If `XDG_CONFIG_HOME` is unset or empty, the user-level directory is `$HOME/.config/linglong/`.

Each directory supports one main configuration file and a set of drop-in files:

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

The main file is optional; you may create only `config.d/`. A drop-in must be an immediate child of `config.d/` and have a `.json` extension. Other files are ignored. File names are sorted lexicographically.

## Loading and Precedence

When an application runs, `ll-cli` loads configuration in the following order. Later entries have higher priority:

| Order | Configuration |
| --- | --- |
| 1 | System-wide global `config.json` |
| 2 | System-wide global `config.d/*.json`, in ascending file-name order |
| 3 | System application `apps/<appid>/config.json` |
| 4 | System application `apps/<appid>/config.d/*.json`, in ascending file-name order |
| 5 | User-wide global `config.json` |
| 6 | User-wide global `config.d/*.json`, in ascending file-name order |
| 7 | User application `apps/<appid>/config.json` |
| 8 | User application `apps/<appid>/config.d/*.json`, in ascending file-name order |
| 9 | Instance configuration selected by `--instance <name>` |

`<appid>` in an application directory must match the application ID passed to `ll-cli run`, for example `org.deepin.calculator`.

If any discovered JSON file is unreadable, has invalid syntax, uses an incorrect field type, or contains an invalid enum value, the run fails instead of skipping that file. After changing a configuration, check its JSON syntax with `jq empty <file>`.

System-level configuration is loaded before user-level configuration. Runtime configuration is suitable for defaults and machine-specific customization; it must not be treated as a mandatory policy for restricting user privileges. Merge behavior, override behavior, and command-line precedence are described under each field below.

## Complete Example

Configuration files must be JSON. Every top-level field is optional. This example shows all currently supported fields:

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

This example demonstrates the format only. Replace host paths, device identifiers, extension IDs, and versions with values that exist on the local machine. JSON strings are not expanded by a shell, so do not use `~`, `$HOME`, or `${HOME}` in paths.

## Field Reference

### `env`

Type: object; values must be strings.

Sets environment variables for container processes. Variables with different names are merged; a key in a later configuration overrides the same key in an earlier one. `ll-cli run --env KEY=VALUE` overrides a key of the same name from configuration files.

An empty object, `{}`, does not remove previously defined variables. To remove a variable, change or remove the earlier file that defines it, or set it to the desired new string in a later configuration.

```json
{
  "env": {
    "LANG": "zh_CN.UTF-8",
    "EXAMPLE_FEATURE": "enabled"
  }
}
```

### `disable_xdp`

Type: Boolean.

Controls XDG Desktop Portal integration in the sandbox:

- `false`: Attempt to enable Portal integration.
- `true`: Disable Portal integration.

An explicit value in a later configuration overrides an earlier value; omitting the field leaves the existing value unchanged. Command-line `--enable-xdp` or `--disable-xdp` takes precedence over every configuration file. If the application ID does not comply with Portal ID rules, `ll-cli` may disable integration automatically unless `--enable-xdp` is used explicitly.

### `enable_pipewire`

Type: Boolean.

When `true`, mounts the current user's PipeWire socket in the container so the application can use audio and video services. When `false`, this additional mount is not enabled.

An explicit value in a later configuration overrides an earlier value; omitting the field leaves the existing value unchanged. Command-line `--enable-pipewire` is applied last and can explicitly enable the feature. There is currently no corresponding command-line option to disable it explicitly.

### `device_mode`

Type: array of strings.

Sets device access modes. The only currently supported value is:

- `"passthru"`: Pass the host device directory through to the container instead of mounting only the default device nodes.

Values from multiple configuration files are appended in loading order without deduplication. An empty array, `[]`, does not clear earlier values. To remove passthrough mode, change or remove the earlier configuration that added `"passthru"`.

### `devices`

Type: array of strings.

Selects devices to add to the container through CDI (Container Device Interface). Each item has this format:

```text
<kind>=<name>
```

For example:

```json
{
  "devices": [
    "nvidia.com/gpu=gpu0"
  ]
}
```

The device must exist in a CDI specification discoverable by `ll-cli`, or the run fails. Lists from multiple configuration files are appended in loading order without deduplication. An empty array, `[]`, does not clear devices added earlier; to remove one, change or remove the earlier configuration that defined it. If `--device` is supplied on the command line, the entire command-line device list takes priority and the configured list is not used.

### `mounts`

Type: array of mount objects.

Each mount object supports:

| Field | Required | Type | Description |
| --- | --- | --- | --- |
| `source` | Yes | String | Source path on the host |
| `destination` | Yes | String | Destination path in the container; an absolute path is recommended |
| `type` | Yes | String | OCI mount type |
| `options` | No | Array of strings | OCI mount options such as `"bind"`, `"rbind"`, `"ro"`, and `"rw"` |

Mounts from multiple configuration files are appended in loading order. An empty array, `[]`, does not clear earlier mounts, and mounts with the same `destination` are not overridden or deduplicated during merging. To remove a mount, change or remove the earlier configuration that defines it. `ll-cli run` currently has no option for adding these persistent mounts directly.

### `ext_defs`

Type: object. Each key is a Base, Runtime, or application reference to extend, and its value is an array of extension definitions.

A target key can be an application ID or include a version, such as `org.deepin.runtime.dtk/23.1.0`. Only definitions matching the ID and version of an actual runtime component participate in extension resolution.

Each extension definition supports:

| Field | Required | Type | Description |
| --- | --- | --- | --- |
| `name` | Yes | String | ID of an installed extension package |
| `version` | Yes | String | Extension version to resolve; an empty string allows any version |
| `directory` | Yes | String | Directory where the extension is mounted in the container |
| `allow_env` | No | Object of strings | Environment variables the extension may modify and their original default values |

If `allow_env` is absent, every environment variable declared by the extension can take effect. If present, it also serves as an allowlist: variables not listed are ignored. When the original environment variable is empty, the mapped value acts as the extension's default when replacing `$ORIGIN`.

Different target keys are merged. If the same target key appears in multiple configurations, its extension definitions are appended in loading order, not overridden or deduplicated by `name`. An empty array, `[]`, does not clear definitions added earlier. To remove a definition, change or remove the earlier configuration. Extensions specified with command-line `--extensions` are added at runtime and do not remove matching definitions from `ext_defs`. Optional extensions that are not installed are skipped.

### `instances`

Type: object. Each key is an instance name, and each value is another runtime configuration object supporting every top-level field on this page.

Different instance names are merged. If an instance with the same name is spread across multiple main files and drop-ins, its fields are merged recursively according to the rules for each field. An empty object, `{}`, does not clear an instance defined earlier.

Instances provide different runtime profiles for one application:

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

A normal run uses `"MODE": "normal"`:

```bash
ll-cli run org.example.demo
```

With `--instance <name>`, system-level and user-level configuration is merged first, then the selected instance configuration is merged at the end. Scalars and environment variables with the same name in the instance therefore have the highest configuration-file priority, while lists continue to be appended. After selecting the instance, `MODE` in the example becomes `debug`:

```bash
ll-cli run --instance development org.example.demo
```

## Minimal Example

To add a read-only directory mount for one application, create:

```bash
mkdir -p "${XDG_CONFIG_HOME:-$HOME/.config}/linglong/apps/org.example.demo/config.d"
```

Then write `50-data.json`:

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

Exit existing instances, then launch the application again:

```bash
ll-cli run org.example.demo
```
