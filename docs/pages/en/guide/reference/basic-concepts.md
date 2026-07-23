# Basic Concepts

This chapter explains how Linyaps works from four perspectives: package kinds, runtime environments, storage and containers, and system architecture. You do not need to master every detail before building your first application. Refer to the relevant sections as needed when choosing a Base or Runtime or diagnosing driver, dependency, container, or repository issues.

## Package Kinds and Use Cases

Linyaps uses `kind` to distinguish package responsibilities. At runtime, the system combines Base, Runtime, App, and Extension as needed instead of installing all content into the host root filesystem.

| Kind | Purpose | Typical use case |
| --- | --- | --- |
| Base | Provides basic system components such as glibc and a shell and forms the container's user-space foundation | Select the base environment to which the application has been adapted |
| Runtime | Provides reusable frameworks and libraries on top of a Base | Environments shared by applications, such as DTK/Qt, Qt WebEngine, and Wine |
| App | Provides application programs, resources, metadata, and launch commands | Desktop and command-line applications or services |
| Extension | Adds runtime content without modifying the App, Runtime, or Base | Optional components such as proprietary graphics drivers that must match host hardware |

### Base

A Base can be understood as a lightweight minimal system image. It lets applications avoid direct dependencies on system libraries provided by the host distribution and is the foundation for cross-distribution execution. A Base is not a complete desktop system and should not carry dependencies specific to individual applications.

### Runtime

A Runtime depends on a Base and provides framework libraries shared by multiple applications. An application should first select a suitable Base and then a compatible Runtime. If it needs only components provided by the Base, it may omit the Runtime.

See [Runtime Components](./runtime.md) for available versions and compatibility.

### App

An App is the application content delivered to users. Build scripts normally install executables, libraries, and resources into `${PREFIX}`, which is mapped to `/opt/apps/<application-id>/files` at runtime. An application declares its entry point with `command` and integrates with the desktop through resources such as desktop files and icons.

### Extension

An Extension adds content on demand so hardware-specific or optional components do not have to be embedded in every application. At runtime, the system can select an appropriate extension automatically, or one can be specified explicitly with `ll-cli run --extensions` or `ll-builder run --extensions`.

Graphics drivers are a typical use case: Mesa is generally provided by the Base, while proprietary or vendor drivers must match the host driver. See [Drivers](./driver.md) for examples.

## Containers and Isolation

Applications are built and run in rootless containers. At runtime, the Base, optional Runtime, App, and Extension are combined into the filesystem visible to the application, while Linux namespaces isolate resources such as processes and mount points. A container is not a virtual machine: it shares the host kernel, so hardware drivers, graphical sessions, D-Bus, and file access must interact with the host through controlled mechanisms.

The application directory is normally read-only at runtime. Configuration, data, and caches should be written to `XDG_CONFIG_HOME`, `XDG_DATA_HOME`, and `XDG_CACHE_HOME` respectively. Resources such as desktop files and icons that must be recognized by the system are exported from the package to the host environment. See the [FAQ](../tips-and-faq/faq.md) for common path issues.

## OSTree and Repositories

Linyaps uses OSTree to store and distribute versioned content. Base, Runtime, App, and Extension packages consist of content-addressed objects, allowing versions to reuse unchanged data. During installation or upgrade, the client fetches metadata and pulls the required objects, avoiding a complete download every time.

After a local build, `ll-builder` commits artifacts to the local build cache. For publishing, content can be exported as a UAB or pushed to a remote repository. On the user side, the package-management service installs, upgrades, and uninstalls repository content. See [Repository Management](../publishing/repositories.md) and [Mirrors](../publishing/mirrors.md).

## Architecture and Toolchain

```text
linglong.yaml / deb / AppImage / Flatpak
                 │
       ll-builder / conversion tools
                 │
        Local OSTree cache ──→ UAB or remote repository
                 │                       │
                 └──── package manager ←┘
                              │
                           ll-cli
                              │
                Base + Runtime + App + Extension
                              │
                        Rootless container
```

- **`ll-cli`** is the user-facing command-line frontend for searching, installing, running, upgrading, and uninstalling applications and managing repositories. It uses the package-management service for persistent package operations and initiates application execution.
- **`ll-builder`** is the developer tool. It reads `linglong.yaml`, prepares the Base, Runtime, and sources, and builds, debugs, validates, exports, and pushes applications in an isolated environment.
- **Package Manager** manages local and remote repositories and installed content, providing installation, upgrade, and uninstall capabilities to `ll-cli`.
- **`ll-pica`** generates a build configuration from a deb and invokes `ll-builder` to complete the conversion. `ll-appimage-convert` and `ll-pica-flatpak` handle AppImage and Flatpak sources respectively.
