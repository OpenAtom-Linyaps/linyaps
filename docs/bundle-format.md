# Bundle Format

## Introduction

Green software refers to binaries that do not install any files to the system.

The basic constraints of green software are:

- Path independence, meaning it has nothing to do with local files
- High compatibility, so the software should contain all dependencies that the system does not have

The well-known technology for building green software on Linux is AppImage.

However, there are many problems with that technology. The biggest problem is that the host system ABI cannot stay stable and differs from one system to another. Linglong cannot solve that problem either. We should maintain a baseline of supported systems or ABIs.

A trick on UnionTech OS or Deepin is to only support systems after UnionTech OS 1020, which reduces the testing work for different base systems.

## Target

The green bundle is not just simple support for running applications independently. It is also designed to be the offline maintainer package format for office system services that support self-running and installation. Therefore, keep the changeability of the bundle format in mind when designing.

When a user has internet access, we mostly use online differential updates or install from repositories. We do not use bundles if we can access the repository when distributing applications.

## Specification

The specification for the export bundle:

- MUST be an ELF file, which can be FlatELF
- MUST be statically-linked
- MUST contain a data segment that can be mounted with FUSE
- MUST contain a loader executable at the root of the FUSE-mounted filesystem

## Implementation

**The implementation changes over time. If you change the implementation, update this section as soon as possible!**

### Prototype

- The bundle is a sample ELF loader that joins a read-only filesystem with cat
- The loader simply calls read_elf64 to calculate the offset of the read-only filesystem
- The loader mounts the read-only filesystem with `squashfuse -o ro,offset=xxx`
- The filesystem is now a read-only filesystem
  - The root of the filesystem should contain a file named loader (by the way, currently it's .loader)
  - The directory structure should be: /{id}/{version}/{arch}/
- The read-only filesystem should support EROFS, and MAY support squashfs

A full example of the filesystem is:

```bash

```
