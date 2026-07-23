<!--
SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# Manage Runtimes with the CLI

This chapter explains how to manage installed runtimes with `ll-cli`. Before continuing, read [Basic Concepts](../reference/basic-concepts.md) to understand the layered relationship among Base, Runtime, and App. The chapter is intended primarily for advanced users who need to diagnose dependencies, control disk usage, or manually control the runtime environment. Runtimes are normally pulled automatically during application installation and shared by multiple applications. They require no separate day-to-day management; use these advanced operations only to diagnose compatibility problems, recover disk space, or repair an abnormal dependency state.

## Inspect Installed Runtime Environments

List installed Runtime and Base packages separately:

```bash
ll-cli list --type=runtime
ll-cli list --type=base
```

Multiple versions of the same Runtime may be installed. When a later operation is ambiguous, use a reference that includes the version:

```text
org.deepin.runtime.dtk/23.1.0
```

A complete reference contains fields such as channel, ID, version, and architecture. See [References (Ref)](../extra/ref.md) for details.

## Remove Unused Runtimes

An "unused Runtime" is one that is not referenced by any installed App. Unreferenced Base packages are also included in the cleanup. Common sources include dependencies left after uninstalling an application and older versions that are no longer used after an upgrade.

Run:

```bash
ll-cli prune
```

`prune` preserves Base, Runtime, and related modules that remain in use according to the dependencies of installed Apps, then directly deletes unreferenced content. It has no preview-only option. The result lists the Base and Runtime packages that were removed.

See [`ll-cli prune`](../reference/commands/ll-cli/prune.md) for all options.

## Analyze Application Dependencies

Show the dependency trees of all installed applications:

```bash
ll-cli analyze depends
```

Analyze one application:

```bash
ll-cli analyze depends org.deepin.calculator
```

The output resembles:

```
main:org.deepin.base/25.2.2.6/x86_64 (base)
├── main:org.deepin.deepin-wine/11.0.0.0/x86_64 (runtime)
│   └── main:com.qq.weixin.work.deepin/5.0.7.0/x86_64 (app)
├── main:org.deepin.runtime.dtk/25.2.2.2/x86_64 (runtime)
│   ├── main:cn.org.linyaps.testsuite.portal/1.0.0.0/x86_64 (app)
│   ├── main:org.blender.lts/4.5.8.0/x86_64 (app)
│   └── main:org.deepin.camera/6.5.39.1/x86_64 (app)
├── main:org.deepin.runtime.webengine/25.2.2.4/x86_64 (runtime)
│   └── main:org.deepin.editor/6.5.47.1/x86_64 (app)
├── main:com.qq.wemeet/3.26.10.404/x86_64 (app)
├── main:com.tencent.wechat/4.1.1.6/x86_64 (app)
├── main:com.windsurf.editor/1.9600.41.0/x86_64 (app)
└── main:org.deepin.driver.media.intel/25.0.0.0/x86_64 (extension)
```

Each dependency tree uses a Base as its root, followed by Runtime, App, and related Extension nodes. Applications that use the same Runtime appear under the same Runtime node. This result shows:

- Which applications use a Runtime.
- Which Base and Runtime versions an application actually resolves to.
- Whether multiple applications share a Runtime.

The argument to `analyze depends` must be an installed App; a Runtime ID cannot be supplied directly. To find the users of a Runtime, run the command without an argument and locate the target Runtime in the complete tree.

The command analyzes the runtime dependency tree formed by content installed on the local machine, not the declared or remote-repository dependency state. For machine-readable output, use the global `--json` option after `ll-cli`:

```bash
ll-cli --json analyze depends
```

## Analyze Disk Usage

Show the space used by all installed modules:

```bash
ll-cli analyze size
```

Linyaps stores content in OSTree, allowing identical data to be shared among Apps, Runtimes, and Base packages. Therefore, simply adding the apparent sizes of their directories is inaccurate. `analyze size` provides these metrics:

| Metric | Meaning |
| --- | --- |
| Exclusive | Data blocks used only by the current module; this space can become directly reclaimable after the module is deleted. |
| Shared | Data blocks shared by the current module and other modules; deleting only this module usually does not free this space. |
| Logical | All space logically used by the current module: Exclusive plus Shared. |
| Actual | Estimated actual use by the current module: all exclusive space plus shared space divided among the modules that use it. |
| Calculated actual total size | Total actual usage calculated after deduplicating the currently installed modules. |
| Repository real size | Actual disk usage of the local repository directory, which may also contain repository metadata, caches, and objects not referenced by current modules. |

Results are sorted by Actual from largest to smallest by default. You can sort by another field:

```bash
ll-cli analyze size --sort exclusive
ll-cli analyze size --sort shared
ll-cli analyze size --sort logical
ll-cli analyze size --sort id
```

Add `--asc` to sort in ascending order:

```bash
ll-cli analyze size --sort actual --asc
```

Sizes in JSON output are expressed in bytes and are suitable for further analysis by scripts:

```bash
ll-cli --json analyze size
```

When estimating cleanup gains, focus on Exclusive and Repository real size. A Runtime with a large Shared or Logical value will not necessarily release the same amount of space when deleted by itself.

## Override an Application's Runtime

To diagnose Runtime compatibility, override the Runtime declared by an application for one launch:

```bash
ll-cli run org.deepin.calculator \
  --runtime main:org.deepin.runtime.dtk/23.1.0/x86_64
```

The specified Runtime must already be installed and must be compatible with the application's Base. This option affects only the current launch. It does not modify application metadata or change the Runtime used for the application's next normal launch.

Overriding a Runtime can introduce ABI incompatibilities, missing dynamic libraries, or changed plugin paths. You can also specify a Base with `--base`, but the Base and Runtime must be compatible with each other.

Multiple Runtime versions can coexist, so installing another version does not overwrite installed versions. To test a different version, first query and install it:

```bash
ll-cli search org.deepin.runtime.dtk --type=runtime --show-all-version
ll-cli install org.deepin.runtime.dtk/23.1.0
```

Then select that version for one launch with `ll-cli run --runtime`. Different versions remain installed until no application references them and `ll-cli prune` removes them, or until they are explicitly uninstalled with force.

## Force-Uninstall a Runtime

A normal uninstall refuses to remove a Base or Runtime directly, protecting dependencies that may still be in use. Always prefer `ll-cli prune` when cleaning up an unused Runtime.

Use forced uninstallation only when Runtime metadata is corrupted, dependencies cannot be repaired normally, or you explicitly need to construct a test environment.

Use a complete version reference:

```bash
ll-cli uninstall \
  main:org.deepin.runtime.dtk/23.1.0/x86_64 \
  --force
```

`--force` bypasses uninstall protection for Base and Runtime packages. It does not automatically uninstall applications that depend on the Runtime or select an alternative Runtime for them. Related applications may fail to start afterward. Reinstall the original Runtime or reinstall the affected applications to restore their dependencies.

If multiple versions of the Runtime are installed and the command does not specify a version, `ll-cli` prompts you to select from the candidates.
