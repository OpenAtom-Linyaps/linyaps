<!--
SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# Linyaps Graphics Driver CDI Adaptation Guide

This guide introduces CDI concepts, the device integration workflow in Linyaps, and how driver adaptors can write configurations, generate specifications, and verify device access in applications.

## Background

An application using a graphics card typically needs three kinds of host resources:

- **Device nodes**, such as `/dev/dri/renderD128`, through which it communicates with the kernel driver.
- **User-space driver files**, such as graphics interface libraries and vendor configuration files.
- **Environment variables** that select a driver implementation or library search path.

A Linyaps application runs in a container with a filesystem isolated from the host. Installing a driver on the host does not by itself provide those resources to the application container.

CDI (Container Device Interface) describes devices and the container configuration they require in JSON or YAML files. A container runtime reads the description and adds device nodes, mounts, and environment variables to the container. A CDI specification does not install a driver; access permissions still depend on host permissions and the identity of the process inside the container. See the [CDI specification](https://github.com/cncf-tags/container-device-interface/blob/main/SPEC.md) for the standard.

In Linyaps, `ll-cdi-drv` generates CDI specifications and default device selections from driver adaptation configurations. `ll-cli` reads these files and injects devices when starting an application. Drivers can also integrate through [extensions](./driver.md).

## Principles

### Describing and Referencing Devices

A CDI specification identifies a device class with `kind`, such as `nvidia.com/gpu`. Its `devices` list assigns a `name` to each selectable device set. Applications refer to devices by their fully qualified name:

```text
<kind>=<name>

nvidia.com/gpu=all
vendor.example/gpu=render
```

A device here is a selectable resource set. It can contain multiple device nodes, mounts, and environment variables, and does not have to correspond to one physical graphics card.

### From Driver Configuration to Application Container

```text
Driver adaptor supplies a generation source configuration
/etc/linglong/cdi-drv/<name>.json
                │
                ▼
          ll-cdi-drv
           ├── CDI specification: describes device resources
           │   /run/linglong-cdi-drv/spec/<name>.json or .yaml
           └── Device defaults: selects a device and binds its specification
               /run/linglong-cdi-drv/cfg/<name>.json
                │
                ▼
            ll-cli run
                │
                ▼
       Device nodes, mounts, and environment variables in the container
```

Each kind of configuration has a specific role:

| File | Contents | Maintained by |
| --- | --- | --- |
| Generation source configuration | Generator, device matching rules, driver paths, and file dependencies | Driver adaptor or administrator |
| CDI specification | Device class, device names, and container edits | `ll-cdi-drv` or the tool it invokes |
| Device defaults | Default device references and their specification paths | `ll-cdi-drv` |

`version: "1"` in the generation source identifies the Linyaps configuration format version. `cdiVersion` in a CDI file identifies the CDI specification version. The builtin generator emits `cdiVersion: "0.6.0"`.

`ll-cdi-drv` provides two generators:

- `command`: invokes a vendor tool and reads the CDI specification from its standard output.
- `linglong`: builds the specification from DRM matching rules, device paths, mounts, and environment variables.

Administrators maintain the generation sources and runtime directory. The standard runtime directory is writable by root and readable by ordinary users. Artifacts under `/run` are regenerated at boot.

## Examples

### A Device in a CDI Specification

This specification illustrates the CDI structure. Its device-node path must match the actual device on the host:

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

`vendor.example/gpu=render` selects the `render` device in this file. Its `containerEdits` describes the device node and environment variable added to the container. Driver libraries and configuration files can also be described through CDI `mounts`.

### Generating a Specification with the NVIDIA Tool

Save the following as `/etc/linglong/cdi-drv/nvidia.json`. The host must provide `/usr/bin/nvidia-ctk` and the corresponding driver:

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

This configuration runs `nvidia-ctk cdi generate` and selects the tool's `all` device as the default. `watchPathChanged` declares that updates to the generator tool and module dependency files trigger a refresh of this configuration.

### Describing Driver Resources with the Builtin Generator

The following `/etc/linglong/cdi-drv/vendor.json` example uses fictional driver names and paths. Replace them with actual values for the driver being integrated:

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

This example selects DRM nodes for `vendor_driver` and adds a device node, driver files, and environment variables. Omitting `drm.nodes` selects both `primary` and `render` nodes by default. Without `deviceProfile` or `defaultDevice`, the configuration filename determines the device name, giving the qualified reference `vendor.example/gpu=vendor-linglong`. Setting `defaultDevice` instead names that single device. Automatic enablement needs no explicit `autoEnable`. Paths are required by default, so `required` is omitted. A missing required path excludes the device from publication.

### Generated Device Defaults

When the NVIDIA tool outputs kind `nvidia.com/gpu` with an `all` device, the corresponding `/run/linglong-cdi-drv/cfg/nvidia.json` contains:

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

`name` is a fully qualified device reference, and `spec` points to the CDI file published with these defaults. `autoEnable: true` includes the device in default application selection. `autoEnable: false` generates an empty `devices` list; the device remains available through an explicit reference.

Different specifications may define the same qualified device name `<kind>=<name>`; all specifications and generated device-default files are retained. At runtime, only the first reference for each qualified name and its bound specification are selected. Edits from other same-named devices are not combined. Skipping a repeated reference emits a warning identifying the retained and skipped specification sources. Auto-enabled references follow the filename order of generated `cfg` files, so resources from same-named devices in different specifications are not applied together.

## Usage

### 1. Prepare the Driver and Source Configuration

Ensure the host has the required kernel driver, user-space libraries, and device nodes. A `command` generator also requires the tool named in `argv`.

Install the source configuration in the configuration directory, for example:

```bash
sudo install -d -m 0755 /etc/linglong/cdi-drv
sudo install -m 0644 nvidia.json /etc/linglong/cdi-drv/nvidia.json
```

Paths in this guide assume a standard system installation. Build and packaging settings determine the configuration directory. `ll-cdi-drv` is installed under the installation prefix's `libexec/linglong/` directory. When executing the commands below, use its actual executable path or ensure it is in the executing user's `PATH`.

### 2. Refresh Manually and Submit Requests

```bash
jq empty /etc/linglong/cdi-drv/nvidia.json
sudo ll-cdi-drv refresh nvidia

cat /run/linglong-cdi-drv/spec/nvidia.yaml
cat /run/linglong-cdi-drv/cfg/nvidia.json
```

The refresh name is the source filename without `.json`. Diagnostics from manual commands are printed to the terminal. Inspect both the diagnostics and artifacts to determine whether the required devices were generated. The builtin generator writes `<name>.json`. The command generator preserves the tool's output and uses `<name>.json` or `<name>.yaml` according to its content. Directory scans only read `.json`, `.yaml`, and `.yml` files, ignoring backup and temporary files with other suffixes. Explicitly selected files have no suffix restriction. Readers try JSON first, then YAML if JSON parsing fails.

| Command | Behavior |
| --- | --- |
| `ll-cdi-drv refresh` | Immediately refresh all configurations |
| `ll-cdi-drv refresh nvidia` | Immediately refresh the watches and artifacts for `nvidia.json` |
| `ll-cdi-drv refresh --request-all` | Submit a full request |
| `ll-cdi-drv refresh --request nvidia` | Submit a request for one configuration |
| `ll-cdi-drv refresh-pending` | Keep consuming requests and refreshing until the idle timeout, which defaults to 5 seconds |

These administrative commands normally run as root. Submitting a request records what needs refreshing without immediately generating artifacts. Run `refresh-pending` manually to process requests, or use the automatic refresh service described below.

The same process handles requests arriving during consumption and resets the idle interval after each refresh. It releases the refresh lock while waiting for new requests, allowing manual refreshes to proceed. Consumption failures return an error without requeuing claimed requests.

`refresh-pending` accepts `--idle-timeout` as a nonnegative integer number of seconds, defaulting to `5`. Setting it to `0` drains requests and exits as soon as the queue is empty. For example, `ll-cdi-drv refresh-pending --idle-timeout 10` exits after the queue has been idle for 10 consecutive seconds.

Immediate refresh and consumption commands accept these options:

| Option | Purpose |
| --- | --- |
| `--config-dir` | Select the source configuration directory |
| `--runtime-dir` | Select the artifact and request queue directory |
| `--unit-dir` | Select the systemd runtime unit directory |
| `--systemctl` | Select the systemctl executable |

Request submission does not accept `--config-dir`. Configuration names must be nonempty, contain only ASCII letters, digits, and `._-`, and must not start with a dot.

### 3. Verify Device Access in an Application

With the default device enabled, start an installed application to use device selection:

```bash
ll-cli run org.example.demo
```

A specification file and device can also be selected explicitly:

```bash
ll-cli run --cdi-spec /run/linglong-cdi-drv/spec/nvidia.yaml \
  --device nvidia.com/gpu=all org.example.demo
ll-cli run org.example.demo -- ls /dev/dri
```

Replace `org.example.demo` with the application ID being tested. Configuration updates apply to subsequently created containers; exit existing instances before verifying changes.

### 4. Automatic Refresh Rules and Setup

Systemd manages automatic refresh with these trigger rules:

| Trigger | Refresh scope |
| --- | --- |
| System boot | All source configurations |
| A source configuration is added, modified, or removed in `/etc/linglong/cdi-drv/` | All source configurations |
| A dependency in a configuration's `watchPathChanged` changes | Only that configuration |
| A refresh request is submitted | All configurations or the individual configuration requested |

Enable refresh at boot, configuration-directory monitoring, and automatic request processing:

```bash
sudo systemctl daemon-reload
sudo systemctl enable --now ll-cdi-drv-config.path ll-cdi-drv-refresh.path ll-cdi-drv-config.service
```

Configuration refresh synchronizes and starts the watches declared by `watchPathChanged`; individual watches do not need manual enablement. After adding, removing, or replacing hardware, an administrator must refresh manually or an external event source must submit a request.

The project's `91-linglong.preset` declares default enablement policy for these three entry points. Installation scripts can apply that policy, or administrators can enable them explicitly with the commands above. Installing a preset file alone does not immediately enable or start units.

### 5. Update, Remove, and Diagnose

When removing a driver, remove its source configuration. A refresh cleans up the corresponding watches and artifacts. To clean up immediately, run this after deleting the source configuration:

```bash
sudo ll-cdi-drv refresh nvidia
```

The maintainer must also update any manually declared device references in ordinary runtime configuration.

To inspect refreshes executed through systemd:

```bash
journalctl -u ll-cdi-drv-config.service -u ll-cdi-drv-refresh.service
systemctl status ll-cdi-drv-config.path ll-cdi-drv-refresh.path
systemctl status ll-cdi-drv-watch@nvidia.path
journalctl -u ll-cdi-drv-request@nvidia.service
```

For missing artifacts, inspect generator output, required paths, and DRM matching conditions. After resolving an error, refresh again or submit another request; failed requests are not retried automatically. If manual refresh works but file changes do not trigger it, check the fixed entry points and the watched paths in the dynamic `.path` units.

## Specification

Source fields are defined in `libs/cdi_drv/schema.json` in the source tree. This section describes configuration semantics and runtime device selection rules.

### Source Configuration Fields

Hidden files and files with invalid configuration names are excluded from configuration enumeration. Each valid `.json` filename identifies an independent configuration. Source loading handles regular files and skips directories and symbolic links.

| Field | Type and meaning |
| --- | --- |
| `version` | Required string with value `"1"` |
| `generator` | Required; `command` or `linglong` |
| `autoEnable` | Boolean, defaults to `true`; controls whether this configuration emits its default device reference |
| `defaultDevice` | Final device name to enable by default; names the single builtin device when device profiles are absent. See below for omission behavior |
| `watchPathChanged` | Absolute dependency paths such as driver files, libraries, and generator tools |
| `argv` | Program and argument array for `command`; the first entry is a nonempty executable name |
| `timeout` | Execution timeout for the `command` generator in integer seconds, default `30`; nonpositive values disable the timeout |
| `kind` | Device class for `linglong`, in `<vendor>/<class>` format |
| `profile` | Shared resources for the `linglong` generator |
| `deviceProfile` | Device configuration array for `linglong`, each containing `device` and `profile` |

### Command Input and Output

`argv` executes the program directly, without a shell. Its executable can be an absolute path or a command found by executable lookup. Standard output provides one valid CDI specification, which determines the kind and device names. Standard error carries diagnostics.

`timeout` limits the total time spent running a command and reading its output, defaulting to 30 seconds. For example, `"timeout": 60` sets a 60-second limit, while zero or a negative value disables the timeout. On timeout, the generator process group is terminated and the direct child is reaped. That source is treated as a generation failure, other sources continue, and claimed requests are not requeued. This timeout is independent of the queue idle interval configured by `refresh-pending --idle-timeout`.

The output can be YAML or JSON. YAML loading uses the first document. `defaultDevice: "all"` selects the device named `all`; the generator's output defines which resources it contains.

### Builtin Resource Rules

The builtin generator requires `kind` and either `profile` or a nonempty `deviceProfile`. If `profile` is absent and `deviceProfile` is absent or empty, it reports the missing resource configuration and skips that configuration.

- The top-level `profile` provides shared resources to each device.
- Each `deviceProfile` entry generates one CDI device, with `device` as its final name.
- When device profiles are absent or empty, the shared profile generates one device named by `defaultDevice`. If omitted, its name is `<configuration name>-linglong`, such as `vendor-linglong`. The name must satisfy CDI device name rules.
- `deviceNodes`, `mountFiles`, and `env` append shared entries before device-specific entries.
- A device-level `drm` replaces the entire shared selector. Omitting it inherits the shared value.
- With a nonempty device profile array, `defaultDevice` must identify a device declared in that array; it does not create an additional default device. If omitted, the first device actually generated is selected. If the selected device fails generation, no default reference for it is emitted.

`drm` is an object describing matching rules:

| Field | Rule |
| --- | --- |
| `kernelDriver` | Optional; filters by kernel driver name when present |
| `pciIds` | Optional or empty for unrestricted IDs. Supports `1234:5678` and `1234:*`; entries are alternative matches |
| `nodes` | An array containing `render` and/or `primary`. Omission selects both; an empty array skips DRM node selection |

When both driver and PCI filters are provided, both must match. Multiple matching nodes can belong to one CDI device.

Device and mount paths follow these rules:

| Entry | Host path | Container path |
| --- | --- | --- |
| `deviceNodes` object | `hostPath`, or `path` when omitted | `path` |
| `mountFiles` string | The string after variable expansion | Same as the host path |
| `mountFiles` object | `hostPath` after variable expansion | `path`, or the host path when omitted |

Mount options are `ro,nosuid,nodev,rbind,rprivate`. Sources can be existing files, directories, or other supported paths. `required` defaults to `true`. A missing required path skips the corresponding device; `required: false` skips only the missing entry. String entries in `mountFiles` are required paths.

A device with no nodes, mounts, or environment variables produces no artifact. A device can still be generated without matching DRM nodes when it has other resources. A configuration with no valid devices emits neither CDI nor device-default artifacts.

`${LIB_DIR}` expands only in host source paths in `mountFiles`, identifying the target system's default library directory. The build variable `LINGLONG_CDI_DRV_LIB_DIR` provides the default, and a nonempty runtime environment variable of the same name overrides it. `$$` denotes a literal `$`, so `$${LIB_DIR}` produces the literal text `${LIB_DIR}`.

### File Dependency Watch Rules

Entries in `watchPathChanged` map to systemd `PathChanged=`. Any declared dependency change submits a request for that configuration. Directory watches are not recursive; appropriate file or subdirectory entries must cover changes within subdirectories. See [systemd.path](https://www.freedesktop.org/software/systemd/man/latest/systemd.path.html) for the semantics.

Paths support systemd specifiers: `%v` is the running kernel release, and `%%` is a literal `%`. Newlines, carriage returns, trailing spaces or tabs, and trailing backslashes are unsupported. The examples watch driver-file and tool dependencies; external event sources or manual refresh handle hardware changes.

inotify does not reliably report content changes to virtual files such as `/sys/module/nvidia/version`. These files cannot substitute for device or kernel module event sources. See [inotify(7)](https://man7.org/linux/man-pages/man7/inotify.7.html).

### Runtime Device Selection Rules

`devices` accepts strings and objects naming a specification file:

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

Strings use `<kind>=<name>`. An object's `name` is the qualified reference, and `spec` binds the specification. See [`ll-cli` runtime configuration](../extra/runtime_config.md) for ordinary configuration locations and application-specific rules. Generated `cfg` files provide device selection; only `devices` is read from them.

In these rules, ordinary configuration means the result of merging system, user, application, and instance configurations in loading order. Their `devices` lists append to each other; an empty array does not clear devices from earlier configurations.

Device selection follows these rules:

1. When `--device` is present, its command-line devices are used. Repeated `--device` options append to each other; only the first command-line reference for each qualified name is selected.
2. Without command-line devices, a nonempty ordinary `devices` list merges with generated defaults. Ordinary entries override all generated references with the same qualified name, and the first reference in the merged ordinary configuration list is selected. Defaults with other qualified names are preserved.
3. An explicit `"devices": []` in the merged ordinary configuration selects no devices, including generated defaults. If an earlier configuration added devices, an empty array in a later configuration cannot make the merged list empty.
4. When ordinary configuration has no `devices`, generated defaults are used. With no device selection, `ll-cli` searches ordinary specification directories for `nvidia.com/gpu=all`. An explicit empty list does not enter this probe.

String references search the active generation's `spec` directory, then `/etc/linglong/cdi`, `/etc/cdi`, and `/var/run/cdi`. `--cdi-spec-dir` selects the ordinary specification directories in priority order. Files within each directory are sorted by filename; the first valid specification matching both `kind` and `name` wins. Adding a higher-priority matching device may change the result of an unbound reference. An object's `spec` and the command-line `--cdi-spec` bind a file directly without directory searching. `--cdi-spec` applies to subsequent `--device` options:

```bash
ll-cli run \
  --cdi-spec /etc/cdi/vendor-a.yaml --device vendor.example/gpu=gpu0 \
  --cdi-spec /etc/cdi/vendor-b.yaml --device vendor.example/gpu=gpu1 \
  org.example.demo
```

Relative `--cdi-spec-dir` paths are interpreted against the invocation's working directory. Specification paths are converted to absolute paths before being stored in the run context, so restoring a context does not depend on the original working directory.

Directory scans skip unreadable or invalid specifications with warnings. Explicit specification read failures and missing selected devices produce errors. Device edits from different specifications apply in the final device-list order.
