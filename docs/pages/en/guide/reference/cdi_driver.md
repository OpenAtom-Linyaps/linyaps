<!--
SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# Linyaps Graphics Driver CDI Adaptation Guide

The runtime container of a Linyaps application is isolated from the host system, so graphics cards and other hardware devices must be injected into the container in a controlled manner. Besides the extension approach described in [Drivers](./driver.md), graphics driver vendors can adopt the CDI (Container Device Interface) approach: devices and their injection details are declared in standardized specification files, and Linyaps adds the devices to the container on demand when an application runs.

The overall workflow for CDI adaptation:

1. The driver package provides a CDI spec generator that produces the specification file and the Linyaps configuration according to the current device state.
2. The driver package installs a systemd service that generates the baseline configuration at boot, and regenerates it on demand when driver files are updated, the spec generator tool is updated, or devices are hot-plugged.
3. When devices are removed or replaced, or the driver is uninstalled, the related configuration is removed or rebuilt accordingly, preventing stale references from breaking application startup.

## Introduction to the CDI Standard

CDI (Container Device Interface) is a community-developed standard for attaching devices to containers, widely supported by mainstream container runtimes. See [container-device-interface](https://github.com/cncf-tags/container-device-interface) for the specification.

The core ideas of CDI:

- Driver vendors describe a class of devices in a JSON or YAML file, including injection details such as device nodes, mounts, and environment variables.
- Specification files are stored in the `/etc/cdi` and `/var/run/cdi` directories (`/var/run` is usually tmpfs and its contents are lost on reboot, so persistent files should be written to `/etc/cdi`), and file names end with `.json`, `.yaml`, or `.yml`.
- Each specification file declares a device class through `kind` in the format `<vendor>/<class>`, for example `nvidia.com/gpu`, where `vendor` must be a valid DNS domain name.
- Devices in a specification are referenced by their fully qualified name in the format `<kind>=<device name>`, for example `nvidia.com/gpu=gpu0`.
- The container runtime injects devices according to the specification, without needing to understand the node rules of each type of hardware.

Benefits of the CDI approach for graphics drivers:

- The driver package only needs to maintain the specification files; neither applications nor Linyaps itself needs to be modified.
- After a device is hot-plugged or replaced, only the specification file needs to be updated; Linyaps picks it up automatically the next time an application runs.
- Linyaps containers run as unprivileged users, and the visibility of and access to device nodes are uniformly controlled through specification files and host udev rules.

## Rules for the devices Field in the Linyaps Configuration

The `devices` field in the `ll-cli` runtime configuration selects the devices added to the container through CDI. See [`ll-cli` runtime configuration](../extra/runtime_config.md) for the complete configuration reference. When adapting a driver, pay attention to the following rules:

| Rule | Description |
| --- | --- |
| Configuration format | An array of strings, each in the format `<vendor>/<class>=<device name>`, for example `vendor.example/gpu=gpu0` |
| Configuration location | System level `/etc/linglong/`, user level `$XDG_CONFIG_HOME/linglong/`; it is recommended to write the driver's own `config.d/*.json` into the system-level configuration directory |
| Device validation | The device must exist in a CDI specification discoverable by `ll-cli`, otherwise running the application fails |

Note in particular: a configuration that explicitly references a nonexistent device causes the application run to fail. If a device reference is written into the system-wide global configuration (such as `/etc/linglong/config.d/`), removing the device affects the startup of all applications. Driver adaptors must therefore keep the configuration in sync with the device state; the generator described below removes the related configuration when no device is detected.

## Generation Examples: CDI Specification and Linyaps Configuration

### Manual Example

Assume the vendor's PCI Vendor ID is `0x1234` and the device class is `vendor.example/gpu`. The CDI specification file `/etc/cdi/vendor-gpu.yaml`:

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

Key fields:

| Field | Description |
| --- | --- |
| `cdiVersion` | CDI specification version; `0.6.0` is currently recommended |
| `kind` | The device class in the format `<vendor>/<class>`, which must match the device reference in the Linyaps configuration |
| `devices[].name` | The device name, which forms the fully qualified reference with `kind`, such as `vendor.example/gpu=gpu0` |
| `devices[].containerEdits.deviceNodes` | The device nodes injected into the container for this device; `path` is the node path on the host |
| `containerEdits` | Spec-level edits that apply to all devices in the file; can declare `env`, `deviceNodes`, `mounts`, and so on |

The corresponding Linyaps configuration is written to `/etc/linglong/config.d/30-vendor-gpu.json` (system-wide, applies to all applications):

```json
{
  "devices": [
    "vendor.example/gpu=gpu0"
  ]
}
```

### Automatic Generation

Hand-written files cannot track device changes. It is recommended that the driver package provide a configuration generator (such as `/usr/bin/vendor-gpu-cdi-generate`) that fully rebuilds or removes the two files according to the current device state each time it is triggered. Key points:

- Filter DRM device nodes of **this vendor or those supported by the current driver** through `/sys/class/drm/*/device/vendor`.
- When no device of this vendor is detected, **remove the CDI specification and the Linyaps configuration to avoid stale references**.
- Keep the Linyaps configuration and the CDI specification consistent with the current device and driver state.

Pseudocode of the generator:

```text
main:
    # 1. Enumerate devices: scan the hardware supported by this driver (for example,
    #    filter this vendor's DRM device nodes via /sys/class/drm/*/device/vendor),
    #    producing a list of device nodes
    nodes = enumerate_supported_device_nodes()

    # 2. Clean up when no device is present: remove the CDI spec and the Linyaps
    #    configuration, then exit, avoiding stale references to nonexistent devices
    if nodes is empty:
        remove spec file and Linyaps configuration file
        return

    # 3. Generate the CDI spec: build devices and containerEdits from the node list,
    #    including device nodes, environment variables, driver library mounts, etc.
    spec = build_cdi_spec(kind, nodes, env, mounts)

    # 4. Generate the Linyaps configuration: each entry of devices is <kind>=<device name>,
    #    and device names match the spec's devices[].name
    linyaps_config = build_devices_list(device names from spec)
```

The generator is installed with `0755` permissions and must be idempotent: no matter which event triggers it, it fully rebuilds or removes the files according to the current device and driver state. For complex topologies such as multiple cards or multiple nodes, generate precisely according to the PCI correspondence in the vendor's tooling; the pseudocode only demonstrates the general flow.

### On-Demand Triggering

The generator is run by a systemd service (Type=oneshot), which generates the baseline configuration at boot and is re-run on demand on various changes:

```ini
# /usr/lib/systemd/system/vendor-gpu-cdi.service
[Unit]
Description=Generate and maintain the vendor GPU CDI spec and Linyaps configuration
After=systemd-modules-load.service local-fs.target

[Service]
Type=oneshot
ExecStart=/usr/bin/vendor-gpu-cdi-generate

[Install]
WantedBy=multi-user.target
```

#### Automatic Triggering

When driver files change, the spec generator tool is updated, or devices change, the generator needs to run again. A systemd path unit can watch the corresponding paths and automatically re-run the service, keeping the CDI specification and the Linyaps configuration consistent with the current device and driver state. Paths commonly watched include:

- **Device directory** (such as `/dev/dri`): any change to the directory contents (creation or deletion of device nodes) triggers, covering device hot-plugging, replacement, and count changes.
- **Driver files and the generator tool** (such as `/usr/lib/vendor-gpu` and `/usr/bin/vendor-gpu-cdi-generate`): covers driver libraries or the tool itself being upgraded or replaced.

```ini
# /usr/lib/systemd/system/vendor-gpu-cdi.path
[Unit]
Description=Watch device and driver file changes and refresh the CDI spec

[Path]
PathChanged=/dev/dri
PathChanged=/usr/lib/vendor-gpu
PathChanged=/usr/bin/vendor-gpu-cdi-generate
Unit=vendor-gpu-cdi.service

[Install]
WantedBy=multi-user.target
```

#### Triggering on Driver Package Installation

Updates to driver files and the configuration generator usually happen when the driver package is upgraded. Within a single driver package, enabling and refreshing the units is handled uniformly by the installation hook, without waiting for path monitoring to notice file changes:

```bash
# postinst of deb / %post of rpm
systemctl daemon-reload
systemctl enable --now vendor-gpu-cdi.path vendor-gpu-cdi.service
```

## Removing Configuration on Device Changes and Uninstallation

### Device Hot-Plugging and Replacement

When devices are unplugged, replaced, or reduced in number, the path unit triggers the service and the generator:

- When all devices disappear: removes `/etc/cdi/vendor-gpu.yaml` and `/etc/linglong/config.d/30-vendor-gpu.json`, so applications no longer reference nonexistent devices.
- When devices are replaced or the count changes: fully rebuilds the specification and the configuration, keeping references such as `gpu0` and `gpu1` always consistent with the actual devices.

Configuration changes only affect applications started afterwards; exit existing application instances before verifying.

### Driver Package Uninstallation

The uninstallation hooks of the driver package (`prerm`/`postrm` of deb, `%preun`/`%postun` of rpm) should stop and disable the on-demand monitoring unit and service, remove the generated files, and reload the unit definitions:

```bash
systemctl disable --now vendor-gpu-cdi.path vendor-gpu-cdi.service
rm -f /etc/cdi/vendor-gpu.yaml /etc/linglong/config.d/30-vendor-gpu.json
systemctl daemon-reload
```

If specification files are written to temporary directories such as `/var/run/cdi`, they disappear after a reboot; files written to `/etc/cdi` must be explicitly removed during uninstallation.

## Verification and Troubleshooting

General troubleshooting approach: first confirm that the generator produces correct files consistent with the current device and driver state, then verify that Linyaps can discover and inject the devices, and finally rule out interference from stale configuration.

### Preliminary Checks

```bash
# View generator logs to confirm the last trigger time and result
journalctl -u vendor-gpu-cdi.service

# View path unit logs to confirm monitoring works and events fire
journalctl -u vendor-gpu-cdi.path

# Check the JSON syntax of the Linyaps configuration
jq empty /etc/linglong/config.d/30-vendor-gpu.json

# Confirm the CDI spec exists and device references match the Linyaps configuration
cat /etc/cdi/vendor-gpu.yaml
cat /etc/linglong/config.d/30-vendor-gpu.json
```

### Verifying Device Injection

```bash
# Specify the device once on the command line to verify the spec works; success means the CDI side is fine
ll-cli run --device vendor.example/gpu=gpu0 org.example.demo

# Confirm the device nodes are injected in the container environment
ll-cli run org.example.demo -- ls /dev/dri

# Confirm the driver library mounts and environment variables are present in the container environment
ll-cli run org.example.demo -- env | grep VENDOR_GPU_RUNTIME

# Enter the container environment for troubleshooting
ll-cli run org.example.demo -- bash
```

### Common Issues

- CDI configuration format or path errors
- Files/devices described in the CDI specification do not exist or have changed
- Linyaps configuration format or path errors
- Stale (residual) configuration
