# Linyaps Documentation Index

This document provides a comprehensive index of all documentation files in the Linyaps project.

## Directory Structure Overview

```
docs/pages/en/guide/
├── building/                    # Build-related documentation
├── debug/                       # Debug-related documentation
├── desktop-integration/         # Desktop integration documentation
├── extra/                       # Additional documentation
├── lessons/                     # Tutorials and examples
├── linyaps-devel/               # Developer documentation
├── publishing/                  # Publishing-related documentation
├── reference/                   # Reference documentation
├── start/                       # Getting started guide
└── tips-and-faq/                # Tips and FAQ
```

## Detailed Documentation Index

### Getting Started (start/)

| Document Name                | File Path                       | Description                                                                                                          |
| ---------------------------- | ------------------------------- | -------------------------------------------------------------------------------------------------------------------- |
| Overview                     | `start/whatis.md`               | Introduction to Linyaps basic concepts, advantages, and comparison with other package management tools               |
| Install Linyaps              | `start/install.md`              | Detailed steps for installing Linyaps on different Linux distributions                                               |
| Build Your First Linyaps App | `start/build_your_first_app.md` | Complete tutorial for building Linyaps packages from source, including calculator example and deb package conversion |
| ll-pica Installation         | `start/ll-pica-install.md`      | Installation guide for ll-pica package conversion tool                                                               |
| Release Notes                | `start/release_note.md`         | Version release notes and changelog                                                                                  |

### Build Related (building/)

| Document Name                         | File Path                          | Description                                                                                                                 |
| ------------------------------------- | ---------------------------------- | --------------------------------------------------------------------------------------------------------------------------- |
| Linyaps Package Specification         | `building/linyaps_package_spec.md` | Detailed application packaging specifications, including naming conventions, build configuration, directory structure, etc. |
| Build Configuration File Introduction | `building/manifests.md`            | Detailed explanation and field definitions for `linglong.yaml` configuration file                                           |
| Module Management                     | `building/modules.md`              | Module splitting and management instructions                                                                                |
| Multi-architecture Support            | `building/multiarch.md`            | Multi-architecture build support instructions                                                                               |
| Demo Example                          | `building/demo.md`                 | Build demonstration example with practical use cases                                                                        |

### Debug Related (debug/)

| Document Name              | File Path        | Description                                                                                     |
| -------------------------- | ---------------- | ----------------------------------------------------------------------------------------------- |
| Debug Linyaps Applications | `debug/debug.md` | Detailed guide for debugging Linyaps applications using gdb, vscode, Qt Creator and other tools |

### Desktop Integration (desktop-integration/)

| Document Name                    | File Path                       | Description                                                       |
| -------------------------------- | ------------------------------- | ----------------------------------------------------------------- |
| Desktop Integration Instructions | `desktop-integration/README.md` | Linyaps and desktop environment integration related documentation |

### Tutorials and Examples (lessons/)

| Document Name        | File Path                         | Description                                               |
| -------------------- | --------------------------------- | --------------------------------------------------------- |
| Basic Notes          | `lessons/basic-notes.md`          | Basic usage notes and best practices                      |
| Build Git Patches    | `lessons/build-git-patch.md`      | Git patch building tutorial for version management        |
| Build in Environment | `lessons/build-in-env.md`         | Tutorial for building in specific environments            |
| Build Offline Source | `lessons/build-offline-src.md`    | Tutorial for building source code in offline environments |
| Test with Toolchains | `lessons/test-with-toolchains.md` | Tutorial for testing with different toolchains            |

### Developer Documentation (linyaps-devel/)

| Document Name           | File Path                 | Description                                        |
| ----------------------- | ------------------------- | -------------------------------------------------- |
| Developer Documentation | `linyaps-devel/README.md` | Developer-related documentation and API references |

### Publishing Related (publishing/)

| Document Name         | File Path                    | Description                                                                      |
| --------------------- | ---------------------------- | -------------------------------------------------------------------------------- |
| Repository Management | `publishing/repositories.md` | Linyaps repository basic concepts, management and publishing update instructions |
| UAB Format Publishing | `publishing/uab.md`          | UAB format publishing instructions and best practices                            |
| Mirrors               | `publishing/mirrors.md`      | Mirror site configuration and management                                         |

### Additional Documentation (extra/)

| Document Name             | File Path                | Description                                       |
| ------------------------- | ------------------------ | ------------------------------------------------- |
| Unit Testing              | `extra/unit-testing.md`  | Unit testing documentation and testing frameworks |
| Bundle Format             | `extra/bundle-format.md` | Bundle format documentation and specifications    |
| System Helper             | `extra/system-helper.md` | System helper documentation and utilities         |
| Application Configuration | `extra/app-conf.md`      | Application configuration documentation           |
| Root Filesystem           | `extra/rootfs.md`        | Root filesystem documentation and management      |
| Reference                 | `extra/ref.md`           | Reference documentation and quick lookup guides   |
| UAB Build                 | `extra/uab-build.md`     | UAB build documentation and build process         |
| README                    | `extra/README.md`        | Additional documentation overview                 |

### Reference Documentation (reference/)

#### Basic Concepts

| Document Name               | File Path                     | Description                                                                 |
| --------------------------- | ----------------------------- | --------------------------------------------------------------------------- |
| Basic Concepts Introduction | `reference/basic-concepts.md` | Introduction to core concepts like Base, Runtime, sandbox, repository, etc. |
| Runtime Component           | `reference/runtime.md`        | Detailed introduction to Runtime component                                  |
| Driver                      | `reference/driver.md`         | Driver-related documentation and specifications                             |

#### Command Reference

**ll-appimage-convert Command**

- `reference/commands/ll-appimage-convert/ll-appimage-convert.md` - Main command description
- `reference/commands/ll-appimage-convert/ll-appimage-convert-convert.md` - convert subcommand

**ll-builder Command**

- `reference/commands/ll-builder/ll-builder.md` - Main command description
- `reference/commands/ll-builder/build.md` - build subcommand
- `reference/commands/ll-builder/create.md` - create subcommand
- `reference/commands/ll-builder/export.md` - export subcommand
- `reference/commands/ll-builder/extract.md` - extract subcommand
- `reference/commands/ll-builder/import.md` - import subcommand
- `reference/commands/ll-builder/list.md` - list subcommand
- `reference/commands/ll-builder/push.md` - push subcommand
- `reference/commands/ll-builder/remove.md` - remove subcommand
- `reference/commands/ll-builder/repo.md` - repo subcommand
- `reference/commands/ll-builder/run.md` - run subcommand

**ll-cli Command**

- `reference/commands/ll-cli/ll-cli.md` - Main command description
- `reference/commands/ll-cli/content.md` - content subcommand
- `reference/commands/ll-cli/enter.md` - enter subcommand
- `reference/commands/ll-cli/info.md` - info subcommand
- `reference/commands/ll-cli/install.md` - install subcommand
- `reference/commands/ll-cli/kill.md` - kill subcommand
- `reference/commands/ll-cli/list.md` - list subcommand
- `reference/commands/ll-cli/prune.md` - prune subcommand
- `reference/commands/ll-cli/ps.md` - ps subcommand
- `reference/commands/ll-cli/repo.md` - repo subcommand
- `reference/commands/ll-cli/run.md` - run subcommand
- `reference/commands/ll-cli/search.md` - search subcommand
- `reference/commands/ll-cli/uninstall.md` - uninstall subcommand
- `reference/commands/ll-cli/upgrade.md` - upgrade subcommand

**ll-pica Command**

- `reference/commands/ll-pica/ll-pica.md` - Main command description
- `reference/commands/ll-pica/install.md` - install subcommand
- `reference/commands/ll-pica/ll-pica-adep.md` - adep subcommand
- `reference/commands/ll-pica/ll-pica-convert.md` - convert subcommand
- `reference/commands/ll-pica/ll-pica-init.md` - init subcommand

**ll-pica-flatpak Command**

- `reference/commands/ll-pica-flatpak/ll-pica-flatpak.md` - Main command description
- `reference/commands/ll-pica-flatpak/ll-pica-flatpak-convert.md` - convert subcommand

### Tips and FAQ (tips-and-faq/)

| Document Name  | File Path                        | Description                                  |
| -------------- | -------------------------------- | -------------------------------------------- |
| Common Issues  | `tips-and-faq/faq.md`            | Common runtime issues and solutions          |
| ll-builder FAQ | `tips-and-faq/ll-builder-faq.md` | Build tool common issues and troubleshooting |
| ll-pica FAQ    | `tips-and-faq/ll-pica-faq.md`    | Package conversion tool common issues        |

## Quick Navigation

### Getting Started for New Users

1. Read [`start/whatis.md`](start/whatis.md) to understand basic concepts
2. Follow [`start/install.md`](start/install.md) to install Linyaps
3. Follow [`start/build_your_first_app.md`](start/build_your_first_app.md) to build your first application

### Developer Reference

- Package Specification: [`building/linyaps_package_spec.md`](building/linyaps_package_spec.md)
- Configuration File: [`building/manifests.md`](building/manifests.md)
- Debug Guide: [`debug/debug.md`](debug/debug.md)

### Command Reference

- Build Tool: [`reference/commands/ll-builder/`](reference/commands/ll-builder/ll-builder.md) directory
- CLI Tool: [`reference/commands/ll-cli/`](reference/commands/ll-cli/ll-cli.md) directory
- Deb Package Conversion Tool: [`reference/commands/ll-pica/`](reference/commands/ll-pica/ll-pica.md) directory
- AppImage Conversion Tool: [`reference/commands/ll-appimage-convert/`](reference/commands/ll-appimage-convert/ll-appimage-convert.md) directory
- Flatpak Conversion Tool: [`reference/commands/ll-pica-flatpak/`](reference/commands/ll-pica-flatpak/ll-pica-flatpak.md) directory

### Common Issues

- Runtime Issues: [`tips-and-faq/faq.md`](tips-and-faq/faq.md)
- Build Issues: [`tips-and-faq/ll-builder-faq.md`](tips-and-faq/ll-builder-faq.md)
- Conversion Issues: [`tips-and-faq/ll-pica-faq.md`](tips-and-faq/ll-pica-faq.md)
