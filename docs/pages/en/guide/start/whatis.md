<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# Overview

linyaps, an open source package format developed by UnionTech Software, is designed to manage, distribute, create sandboxes for packages, and integrate development tools, instead of using package management tools such as `deb` or `rpm`.

## Current Issues with Package Managers

1. Both `deb` and `rpm` are strongly-dependent package management systems that allow complex cross dependencies (or circular dependencies) between components, which makes maintenance a matter of great expertise. A little carelessness will lead to a complete system failure that cannot be repaired.
2. Installation dependencies and running dependencies are coupled so that multiple versions can hardly coexist. Although `deb` and `rpm` have provided several solutions to solve the multi-version coexistence problem, these solutions require changes in source code and are infeasible.
3. The `Hook` system is complex and has no restrictions, through which many operations can damage the system.
4. They have insufficient reliability, no redundant recovery design, and a lack of verification mechanisms. Once the package management system fails, the system can hardly be repaired.
5. The permissions of `deb` and `rpm` are loosely controlled with significant security risks.
6. The current package updates do not support incremental updates, which is a great waste of resources.

## Advantages of Linyaps

1. Improve usability and solve the dependency conflict problem of `deb` and `rpm`.
2. Implement the application permission management mechanism to strengthen security.
3. Support incremental updates of applications.

## Comparison

| Feature                                     | Linyaps                         | Flatpak                   | Snap                       | AppImage                                             |
| ------------------------------------------- | ------------------------------- | ------------------------- | -------------------------- | ---------------------------------------------------- |
| Desktop Application Packaging               | ✔                              | ✔                        | ✔                         | ✔                                                   |
| Terminal Application Packaging              | ✔                              | ✔                        | ✔                         | ✔                                                   |
| Server Application Handling                 | ✔                              | ✘                         | ✔                         | ✘                                                    |
| System Service Packaging (root privileges)  | ✘                               | ✘                         | ✔                         | ✘                                                    |
| Theme Functionality Normal                  | ✔                              | ✔                        | ✔                         | ✔                                                   |
| Library Hosting Service Provided            | ✔                              | ✘                         | ✘                          | ✘                                                    |
| Library/Dependency Source                   | Package carries its own         |                           |                            |                                                      |
| Host System                                 | Package carries its own         |                           |                            |                                                      |
| SDK                                         | Package carries its own         |                           |                            |                                                      |
| Snap Base                                   |                                 |                           |                            |                                                      |
| Commercial Support                          | ✔                              | ✘                         | ✔                         | ✘                                                    |
| Number of App Stores                        | Estimated 4700+                 | 1400+                     | 6600+                      | 1300+                                                |
| Development Tool Support                    | linglong-builder                | GNOME Builder             | electron-builder           |                                                      |
| Container Support                           | ✔                              | ✔                        | ✔                         | ◐ (Not officially provided, technically feasible)    |
| Rootless Container                          | ✔                              | ✘                         | ✘                          | ✘                                                    |
| Run Without Installation                    | ✔ (Provides Bundle Mode)       | ✘                         | ✘                          | ✔                                                   |
| Run Without Extraction                      | ✔ (Provides Bundle Mode)       | ✘                         | ✔                         | ✔                                                   |
| Self-distribution/Green Format Distribution | ✔                              | ✘                         | ✘                          | ✔                                                   |
| Wine Application Support                    | ✔                              | ◐ (Theoretically feasible | ◐ (Theoretically feasible) | ◐ (Uses LD to modify open calls, poor compatibility) |
| Offline Environment Support                 | ✔                              | ✔                        | ✔                         | ✔                                                   |
| Permission Management                       | ✔                              | ✔                        | ✔                         | ✘                                                    |
| Central Repository                          | mirror-repo-linglong.deepin.com | FlatHub                   | Snap Store                 | AppImageHub                                          |
| Multi-version Coexistence                   | ✔                              | ✔                        | ✔                         | ✔                                                   |
| Peer-to-peer Distribution                   | ✔                              | ✔                        | ✔                         | ✔                                                   |
| Application Upgrade                         | Repository upgrade              | Repository upgrade        | Repository upgrade         | Official tool upgrade                                |
