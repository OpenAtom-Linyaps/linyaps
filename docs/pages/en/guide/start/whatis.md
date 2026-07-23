<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# Overview

Linyaps is an open source package format developed by UnionTech Software. It defines a new application-management paradigm that covers the entire lifecycle from development and distribution to execution.

## Getting Started

If you are using Linyaps for the first time, start with the [Quick Start](./quick-start.md) to learn how to search for, install, and run applications.

If you are an application developer who wants to understand the Linyaps package format or distribute an application in that format, start with [Build Your First Linyaps Application](./build_your_first_app.md) and complete the full build process.

## Why Linyaps Is Needed

Traditional package-management systems have the following limitations:

* Maintaining correct and stable system dependencies usually requires professional system maintainers, making them poorly suited to independent application distribution.
* Application installation has excessive privileges and can easily compromise system stability.
* Systems and applications are tightly coupled. It is difficult for a system to evolve continuously while providing a stable ABI, making application availability and development and maintenance cycles unpredictable.
* Different distributions use different package formats, requiring developers to repeat packaging and adaptation work at a high maintenance cost.

Linyaps provides the following advantages:

* Based on container technology, Linyaps provides a stable base environment and isolates the system from the application's runtime environment, so users do not need to worry about installed software breaking system dependencies.
* Applications can be built once and run anywhere, greatly reducing cross-distribution and cross-version maintenance costs.
* The entire toolchain runs as a non-root user by default. Combined with rootless containers, this reduces the attack surface and improves system security.
* Incremental application updates are supported.

## Comparison

| Feature | Linyaps | Flatpak | Snap | AppImage |
| --- | --- | --- | --- | --- |
| Package desktop applications | ✔ | ✔ | ✔ | ✔ |
| Package terminal applications | ✔ | ✔ | ✔ | ✔ |
| Handle server applications | ✔ | ✘ | ✔ | ✘ |
| Package system services (root privileges) | ✘ | ✘ | ✔ | ✘ |
| Themes work correctly | ✔ | ✔ | ✔ | ✔ |
| Provide a library-hosting service | ✔ | ✘ | ✘ | ✘ |
| Library/dependency source | Included in package | | | |
| Host system | Included in package | | | |
| SDK | Included in package | | | |
| Snap Base | | | | |
| Commercial support | ✔ | ✘ | ✔ | ✘ |
| Number of applications in store | Estimated 4,700+ | 1,400+ | 6,600+ | 1,300+ |
| Development-tool support | linglong-builder | GNOME Builder | electron-builder | |
| Container support | ✔ | ✔ | ✔ | ◐ (not officially provided, technically feasible) |
| Rootless containers | ✔ | ✘ | ✘ | ✘ |
| Run without installation | ✔ (Bundle mode) | ✘ | ✘ | ✔ |
| Run without extraction | ✔ (Bundle mode) | ✘ | ✔ | ✔ |
| Self-distribution/portable-format distribution | ✔ | ✘ | ✘ | ✔ |
| Wine application support | ✔ | ◐ (theoretically feasible) | ◐ (theoretically feasible) | ◐ (modifies `open` calls through LD; poor compatibility) |
| Offline environment support | ✔ | ✔ | ✔ | ✔ |
| Permission management | ✔ | ✔ | ✔ | ✘ |
| Central repository | mirror-repo-linglong.deepin.com | Flathub | Snap Store | AppImageHub |
| Multiple versions coexist | ✔ | ✔ | ✔ | ✔ |
| Peer-to-peer distribution | ✔ | ✔ | ✔ | ✔ |
| Application upgrades | Repository upgrade | Repository upgrade | Repository upgrade | Official tool upgrade |

## Documentation Structure and Reading Guide

- **Getting Started** is intended for users and application developers who are new to Linyaps. It introduces Linyaps, installation methods, and basic workflows. End users can begin with Quick Start, while developers can continue to Build Your First Linyaps Application.
- **User Guide** covers routine operations such as installing, running, upgrading, and uninstalling applications, configuring repositories and mirrors, and troubleshooting common problems.
- **Developer Documentation** covers the application packaging specification, `linglong.yaml`, module management, multi-architecture builds, debugging, CI, and distribution. Complete the developer introduction first, then consult individual topics as needed.
- **Reference Documentation** covers fundamental concepts such as Base, Runtime, Extension, containers, and OSTree, together with command references for tools including `ll-cli`, `ll-builder`, and `ll-pica`. It is designed for lookup rather than sequential reading.
- **Selected Tutorials** provides complete build exercises, including compiling applications in containers or locally, managing source code with Git and patches, and using the automated test suite. Continue with these after the developer introduction.

## Getting Help

If you encounter a problem while using, building, or running an application, first consult [Tips and FAQ](../tips-and-faq/faq.md). If you still need help, use one of the following channels and include your system version, Linyaps version, complete command, and error log.

- Users and developers can ask questions and exchange ideas in the [deepin Community](https://bbs.deepin.org).
- After confirming that an issue is reproducible, developers can report it through [GitHub Issues](https://github.com/OpenAtom-Linyaps/linyaps/issues). Follow the issue template and provide reproduction steps, the expected and actual results, and environment information.
