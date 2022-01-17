# Bundle Format

## Introduce

Green software, known as some binary does not install any file to the system.

The base constraint of green software is:

- Path unrelated, means has nothing to do with local files.

- highly compatibility, so the software should contain all dependencies that the system does not have.

The famous technology to build green software is AppImage on Linux.

However, there are many problems with that technology, the biggest problem is that the host system ABI can not stay stable and differ from one another. The linglong can not solve that problem either. We should keep a baseline of support system or ABI.

A trick on uniontech os or deepin is just support system after uniontech os 1020, which reduces the testing work for the different base systems.

## Target

The green bundle is not just simple support for application run path independently. It is also designed to be the offline maintainer package format on office system service package support self-run and install. so keep the changeability of the bundle format when designing.

When a user has internet, we mostly use online diff update or install by repo. we do not use bundle if we can access the repo when distributing applications.

## Specification

The spec of the export bundle:

- MUST be an ELF, can be FlatELF
- MUST be statically-linked
- MUST contain a data segment that can mount with fuse
- MUST contain a loader executable on the root of fuse mount filesystem

## Implementation

**The implementation change with time，If you change the implement, update this selection as soon as possible !!!**

### prototype

- The bundle is a sample elf loader that joins a squashfs with cat.
- The loader simply calls read_elf64 to calc the offset of squashfs.
- The loader mount squashfs with `squashfuse -o ro,offset=xxx`
- The filesystem now is squashfs
  - The root of the filesystem should contain a file name loader(by the way, now is .loader)
  - the directory structure should be: /{appId}/{version}/{arch}/

a full example of the filesystem is:

```bash

```
