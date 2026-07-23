# Linyaps Documentation Index

This document lists the documents currently included in the navigation outline of the English `guide` directory. Documents are grouped by audience into Getting Started, User Guide, and Developer Guide, while files remain organized by content type.

## Directory Structure

```txt
docs/pages/en/guide/
├── building/                    # Build documentation
├── debug/                       # Debugging documentation
├── desktop-integration/         # Desktop integration documentation
├── extra/                       # Additional documentation
├── lessons/                     # Selected tutorials
├── linyaps-devel/               # Developer documentation
├── publishing/                  # Publishing documentation
├── reference/                   # Reference documentation
├── start/                       # Getting started
└── tips-and-faq/                # Tips and frequently asked questions
```

## Documentation Outline

### Getting Started

Getting Started is expanded by default and helps readers quickly understand and use Linyaps.

| Document | File | Summary |
| --- | --- | --- |
| Overview | `start/whatis.md` | Introduces Linyaps, its features and use cases, and how to use the documentation |
| Quick Start | `start/quick-start.md` | Explains how to prepare `ll-cli`, then search for, install, and run applications |
| Release Notes | `start/release_note.md` | Summarizes new features, bug fixes, and other important changes in each release |

### User Guide

The User Guide is collapsed by default and covers installation, application management, and advanced runtime management.

| Level | Document | File | Summary |
| --- | --- | --- | --- |
| Basic | Install Linyaps | `start/install.md` | Introduces repository types and installation on different Linux distributions |
| Basic | Manage Applications | `start/manage-apps-with-cli.md` | Covers installing, inspecting, upgrading, uninstalling, and managing application processes |
| Advanced | Runtime Configuration | `extra/runtime_config.md` | Describes configuration locations, load order, a complete example, and field meanings |
| Advanced | Manage Runtimes | `start/manage-runtimes-with-cli.md` | Covers runtime inspection, cleanup, dependency analysis, overrides, and forced removal |
| Advanced | Repository Management | `publishing/repositories.md` | Explains repository configuration and operations such as adding, reprioritizing, and deleting repositories |

### Developer Guide

The Developer Guide is collapsed by default and covers application building, publishing, desktop integration, and advanced development topics.

| Level | Document | File | Summary |
| --- | --- | --- | --- |
| Basic | Build Your First Linyaps Application | `start/build_your_first_app.md` | Completes a first application build by configuring metadata, dependencies, source code, and build scripts |
| Basic | ll-builder Workflow | `start/ll-builder-workflow.md` | Introduces project creation, configuration, building, debugging, validation, exporting, and pushing |
| Build Example | Understanding the Build Configuration | `building/demo.md` | Walks through the package, dependency, source, and build sections of `linglong.yaml` |
| Build Example | Convert a deb Package | `building/deb_conversion.md` | Shows how to unpack a deb, organize files, add dependencies, and generate a Linyaps application |
| Basic | Publish in UAB Format | `publishing/uab.md` | Introduces publishing in UAB format; content is yet to be added |
| Basic | Desktop Integration Guide | `desktop-integration/README.md` | Introduces desktop integration through Portals and related mechanisms; content is yet to be added |
| Advanced | Debug a Linyaps Application | `debug/debug.md` | Introduces preparation of the debugging example and debugging an application with GDB |
| Advanced | Module Management | `building/modules.md` | Explains module splitting, module files, retained modules, and building and installing modules |
| Advanced | Multi-architecture Support | `building/multiarch.md` | Introduces supported architectures, project configuration, build commands, and cross-building |

## Recommended Reading Order

### General Users

1. Overview
2. Quick Start
3. Install Linyaps
4. Manage Applications
5. Read Runtime Configuration, Manage Runtimes, and Repository Management as needed

### Application Developers

1. Overview
2. Build Your First Linyaps Application
3. ll-builder Workflow
4. Read Understanding the Build Configuration or Convert a deb Package
5. Read publishing, desktop integration, and advanced development documentation as needed
