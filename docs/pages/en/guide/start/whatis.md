<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# Summary

Linglong, an open source package format developed by UnionTech Software, is designed to manage, distribute, create a sandbox for packages, and integrate development tools, instead of using package management tools such as `deb` or `rpm`.

## Problems with current package management

1. Both `deb` and `rpm` are strongly-dependent package management systems and allow complex cross dependencies (or circular dependencies) between components, which makes maintenance a matter of great expertise. A little carelessness will lead to a complete system failure that cannot be repaired.
2. Installation dependencies and running dependencies are coupled so that multiple versions can hardly coexist. Although `deb` and `rpm` have provided several solutions to solve the multi-version coexistence problem, these solutions require changes in source code and are infeasible.
3. The `Hook` system is complex and has no restrictions, through which many operations can damage the system.
4. They have insufficient reliability, no redundant recovery design, and a lack of verification mechanisms. Once the package management system fails, the system can hardly be repaired.
5. The permissions of `deb` and `rpm` are loosely controlled with big security risks.
6. The current package updates do not support incremental updates, which is a great waste of resources.

## Linglong advantages

1. Improve usability and solve the dependency conflict problem of `deb` and `rpm`.
2. Perform the application permission management mechanism to strengthen security.
3. Support incremental updates of applications.

## Comparison

| Features                                    | Linglong                                        | Flatpak                     | Snap                        | AppImage                                                  |
| ------------------------------------------- | ----------------------------------------------- | --------------------------- | --------------------------- | --------------------------------------------------------- |
| Package desktop apps                        | ✔                                              | ✔                          | ✔                          | ✔                                                        |
| Package terminal apps                       | ✔                                              | ✔                          | ✔                          | ✔                                                        |
| Deal with server apps                       | ✔                                              | ✘                          | ✔                          | ✘                                                        |
| Package system services (root access)       | ✘                                              | ✘                          | ✔                          | ✘                                                        |
| Normal themes                               | ✔                                              | ✔                          | ✔                          | ✔                                                        |
| Library hosting services                    | ✔                                              | ✘                          | ✘                          | ✘                                                        |
| Source of libraries/dependencies            | In packages                                     |                             |                             |                                                           |
| Host system                                 | In packages                                     |                             |                             |                                                           |
| SDK                                         | In packages                                     |                             |                             |                                                           |
| snap base                                   |                                                 |                             |                             |                                                           |
| Commercial support                          | ✔                                              | ✘                          | ✔                          | ✘                                                        |
| Apps quantity                               | About 3000+                                     | 1400+                       | 6600+                       | 1300+                                                     |
| Development tools                           |                                                 | GNOME Builder               | electron-builder            |                                                           |
| GNOME Builder                               | electron-builder                                |                             |                             |                                                           |
| Sandbox                                     | ✔                                              | ✔                          | ✔                          | ◐ (Not officially available, but technically feasible)   |
| Rootless sandbox                            | ✔                                              | ✘                          | ✘                          | ✘                                                        |
| Run without installation                    | ✔ (Offer Bundle packages)                      | ✘                          | ✘                          | ✔                                                        |
| Run without decompression                   | ✔ (Offer Bundle packages)                      | ✘                          | ✔                          | ✔                                                        |
| Self-distribution/Green-format distribution | ◐ (Technically feasible, but system limits it) | ✘                          | ✘                          | ✔                                                        |
| Run Wine apps                               | ◐ (Adapting now)                               | ◐ (Theoretically possible) | ◐ (Theoretically possible) | ◐ (Use LD to modify open calls, with poor compatibility) |
| Support offline environment                 | ✔                                              | ✔                          | ✔                          | ✔                                                        |
| Permission management                       | ✔                                              | ✔                          | ✔                          | ✘                                                        |
| Center repository                           | mirror-repo-linglong.deepin.com                 | FlatHub                     | Snap Store                  | AppImageHub                                               |
|                                             |                                                 |                             |                             |                                                           |
| Multi-version coexistence                   | ✔                                              | ✔                          | ✔                          | ✔                                                        |
| Peer-to-peer distribution                   | ✔                                              | ✔                          | ✔                          | ✔                                                        |
| App upgrades                                | By repository                                   | By repository               | By repository               | By official tool                                          |
