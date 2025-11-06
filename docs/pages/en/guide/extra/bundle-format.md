# Bundle Format

## Introduction

Green software refers to binaries that do not install any files to the system.

The fundamental constraints of green software are:

- **Path independence**: Has nothing to do with local files.
- **High compatibility**: The software should contain all dependencies that the system does not have.

The well-known technology for building green software is AppImage on Linux.

However, there are many problems with this technology. The biggest issue is that the host system ABI cannot remain stable and differs from one another. Linyaps cannot solve this problem either. We should maintain a baseline of supported systems or ABI.

A strategy used on UnionTech OS or Deepin is to only support systems after UnionTech OS 1020, which reduces testing work for different base systems.

## Target

The green bundle is not just simple support for application runtime path independence. It is also designed to be the offline maintenance package format on the office system, supporting self-run and installation while maintaining the changeability of the bundle format when designing.

When a user has internet access, we mostly use online differential updates or installation via repo. We do not use bundles if we can access the repo when distributing applications.

## Specification

The specifications for export bundles:

- **MUST** be an ELF, can be FlatELF
- **MUST** be statically-linked
- **MUST** contain a data segment that can be mounted with FUSE
- **MUST** contain a loader executable at the root of the FUSE mount filesystem

## Implementation

**The implementation changes over time. If you change the implementation, update this section as soon as possible !!!**

### Prototype

- The bundle is a simple ELF loader that combines a readonly filesystem with cat.
- The loader simply calls read_elf64 to calculate the offset of the readonly filesystem.
- The loader mounts the readonly filesystem with `squashfuse -o ro,offset=xxx`
- The filesystem is now a readonly filesystem
  - The root of the filesystem should contain a file named loader (currently .loader)
  - The directory structure should be: `/{id}/{version}/{arch}/`
- The readonly filesystem should support EROFS, and MAY support SquashFS

A full example of the filesystem is:

```bash

```
