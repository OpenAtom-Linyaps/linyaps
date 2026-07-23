<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# Understanding a Build Configuration Example

This chapter uses [`linglong.yaml`](https://github.com/OpenAtom-Linyaps/linyaps/blob/master/linglong.yaml) from the `master` branch of the Linyaps repository to explain how a real project organizes package information, sources, build commands, and build dependencies. For line-by-line comparison, the excerpts are pinned to commit [`2bd8bf49`](https://github.com/OpenAtom-Linyaps/linyaps/blob/2bd8bf49ec8e34c52cf6dd296829cc0e0caa610b/linglong.yaml).

Each section quotes the original configuration unchanged before explaining its fields and commands. Because `master` changes continuously, follow the upstream file if this page differs from it. See [Introduction to the Build Configuration File](./manifests.md) for complete field definitions.

## Header and Format Version

```yaml
# SPDX-FileCopyrightText: 2025 - 2026 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later
version: '1'
```

The first three lines are SPDX copyright and license declarations. `version: '1'` selects version 1 of the `linglong.yaml` format; it is not the application version.

## Package Information

```yaml
package:
  id: cn.org.linyaps.builder.utils
  name: ll-builder-utils
  version: 0.0.3.0
  kind: app
  description: |
    Utils for ll-builder
```

| Field | Original value | Description |
| --- | --- | --- |
| `id` | `cn.org.linyaps.builder.utils` | Unique package identifier in reverse-domain notation |
| `name` | `ll-builder-utils` | Package name |
| `version` | `0.0.3.0` | Version built by this configuration |
| `kind` | `app` | Indicates that the package has an executable entry point |
| `description` | `Utils for ll-builder` | Describes utilities provided for `ll-builder` |

Although `ll-builder-utils` is not a desktop application, it provides executable commands and therefore uses `kind: app`.

## Launch Command

```yaml
command: [/opt/apps/cn.org.linyaps.builder.utils/files/bin/ll-builder-export]
```

`command` specifies the default package entry point. It launches `/opt/apps/cn.org.linyaps.builder.utils/files/bin/ll-builder-export`, so that executable must exist in the build result.

`/opt/apps/cn.org.linyaps.builder.utils/files` is the application's directory in the runtime environment and corresponds to `$PREFIX` during the build.

## Base

```yaml
base: org.deepin.base/25.2.0
```

The configuration uses `org.deepin.base/25.2.0` as its base environment. The Base provides fundamental components required for building and running.

There is no `runtime` field, meaning that the package needs no additional Runtime. See [Runtime Components](../reference/runtime.md) for Base and Runtime selection.

## Sources

```yaml
sources:
  - kind: archive
    url: https://github.com/erofs/erofs-utils/archive/refs/tags/v1.8.6.tar.gz
    digest: 5b221dc3fd6d151425b30534ede46fb7a90dc233a8659cba0372796b0a066547
    name: erofs-utils
  - kind: archive
    url: https://github.com/libfuse/libfuse/releases/download/fuse-3.17.1/fuse-3.17.1.tar.gz
    digest: 2d8ae87a4525fbfa1db5e5eb010ff6f38140627a7004554ed88411c1843d51b2
    name: fuse
  - kind: archive
    url: https://github.com/OpenAtom-Linyaps/linyaps-box/archive/refs/tags/2.1.2.tar.gz
    digest: 70c908e3a2397d195d64d606b886c9635b13a461beea70a6cc5317e0bb6e9589
    name: linyaps-box
```

The configuration declares three `archive` sources:

| `name` | Version in URL | Build use |
| --- | --- | --- |
| `erofs-utils` | `v1.8.6` | Build EROFS- and FUSE-related tools |
| `fuse` | `3.17.1` | Build the static FUSE library |
| `linyaps-box` | `2.1.2` | Build static `linyaps-box` |

`url` is the archive address, `digest` verifies the download, and `name` determines the source-directory name under `/project/linglong/sources`.

An extracted archive may still contain a versioned directory. For example, `name: fuse` is built from `/project/linglong/sources/fuse/fuse-3.17.1`. When changing a source version, update the URL, digest, and paths used by `build` together.

## Build Process

The build script performs the following operations in dependency order.

### Print the Installation Prefix

```yaml
build: |
  echo "$PREFIX"

```

`echo "$PREFIX"` prints the application's installation prefix so the build log shows where final artifacts should be installed.

### Build the Static FUSE Library

```yaml
  # build libfuse static library
  cd /project/linglong/sources/fuse/fuse-3.17.1
  patch lib/mount.c /project/apps/ll-builder-utils/patch/libfuse.patch
  mkdir build || true
  cd build
  meson setup ../
  meson configure --default-library static -D utils=false -D examples=false -D tests=false -D disable-libc-symbol-version=false
  ninja && ninja install

```

1. Enter the `fuse-3.17.1` source directory.
2. Modify `lib/mount.c` with the project's `apps/ll-builder-utils/patch/libfuse.patch`.
3. Create and enter `build`; `mkdir build || true` continues if the directory exists.
4. Initialize it with `meson setup ../`.
5. Configure static libraries, disable utilities, examples, and tests, and set the libc symbol-version option explicitly.
6. Build with `ninja` and install only after a successful build.

This produces the static FUSE library required by later components.

### Build the Static erofsfuse Library

```yaml
  # build erofsfuse static library
  cd /project/linglong/sources/erofs-utils/erofs-utils-1.8.6
  ./autogen.sh
  ./configure -with-libzstd --enable-fuse --enable-static-fuse --with-libdeflate --without-xxhash libdeflate_LIBS=-ldeflate libdeflate_CFLAGS=-ldeflate
  make -j$(nproc)
  make install

```

1. Enter the `erofs-utils-1.8.6` source directory.
2. Run `./autogen.sh` to generate the Autotools build files.
3. The original `./configure` enables zstd, FUSE, static FUSE, and libdeflate, disables xxHash, and sets libdeflate link and compile arguments explicitly.
4. Build in parallel with the available CPU cores.
5. Install the result for later steps.

### Build Static linyaps-box

```yaml
  # build static ll-box
  cd /project/linglong/sources/linyaps-box/linyaps-box-2.1.2/
  cmake --preset static
  cmake --build build-static -j$(nproc)
  cmake --install build-static --prefix=$PREFIX

```

1. Enter the `linyaps-box-2.1.2` source directory.
2. Configure with the project's `static` CMake preset.
3. Build `build-static` in parallel.
4. Install into the current application's `$PREFIX`.

### Build the Current Linyaps Project

```yaml
  cd /project
  cmake -B build-linglong -DENABLE_CPM=false -DENABLE_TESTING=false -DBUILD_LINGLONG_BUILDER_UTILS_IN_BOX=true -DAGGRESSIVE_UAB_SIZE=ON
  cmake --build build-linglong -j$(nproc)
  cmake --install build-linglong --prefix=$PREFIX
  install /usr/local/bin/mkfs.erofs $PREFIX/bin/
```

1. Return to the Linyaps source root with `cd /project`.
2. Configure CMake with CPM and tests disabled, Builder Utils enabled inside the container, and `AGGRESSIVE_UAB_SIZE=ON`.
3. Build the current project in parallel.
4. Install the result into the current application directory.
5. Collect the previously built `mkfs.erofs` in the final package's `bin` directory.

`ll-builder-export`, which is referenced by `command`, and every other required runtime tool must enter `$PREFIX` during final installation.

## Build Dependencies

```yaml
buildext:
  apt:
    build_depends:
      [
        patch,
        meson,
        libtool,
        pkg-config,
        uuid-dev,
        libdeflate-dev,
        libzstd-dev,
        nlohmann-json3-dev,
        libyaml-cpp-dev,
        liblz4-dev,
        liblzma-dev,
        libselinux1-dev,
        libpcre2-dev,
        libelf-dev,
        libcap-dev,
        libcli11-dev,
        libgtest-dev,
        libsystemd-dev,
        libfmt-dev,
        libexpected-dev
      ]
```

`buildext.apt.build_depends` declares deb packages installed in the build environment:

- `patch`, `meson`, `libtool`, and `pkg-config` provide build tools.
- `uuid-dev`, `libdeflate-dev`, `libzstd-dev`, `liblz4-dev`, and `liblzma-dev` provide filesystem- and compression-related development files.
- `nlohmann-json3-dev`, `libyaml-cpp-dev`, `libselinux1-dev`, `libpcre2-dev`, `libelf-dev`, `libcap-dev`, `libcli11-dev`, `libsystemd-dev`, `libfmt-dev`, and `libexpected-dev` provide required development libraries.
- `libgtest-dev` provides GoogleTest development files.

These packages are for building and do not automatically become final application content merely because they appear in `build_depends`. Files distributed with the application must still be installed into `$PREFIX` by `build`.

## Summary

A source build normally requires `build` to:

1. **Enter source directories** based on `sources.name` and the extracted archive layout.
2. **Prepare sources** by applying patches, generating `configure`, or performing other project-specific preparation.
3. **Configure the build** with CMake, Meson, Autotools, qmake, or the project's existing build system, setting the installation prefix to `${PREFIX}`.
4. **Compile sources**, optionally in parallel with `$(nproc)`.
5. **Install artifacts** such as executables, libraries, and resources into `${PREFIX}`, not directly into `/usr` in the build container.
6. **Collect extra files** such as tools, desktop files, icons, or resources that the build system did not install automatically.

Standard build and installation procedures can usually be reused:

```bash
# CMake
cmake -S . -B build -DCMAKE_INSTALL_PREFIX=${PREFIX}
cmake --build build
cmake --install build

# Meson
meson setup build --prefix=${PREFIX}
meson compile -C build
meson install -C build

# Autotools
./configure --prefix=${PREFIX}
make -j$(nproc)
make install
```

If source code supports a conventional build system and configurable installation prefix, its existing commands can usually be moved into the `build` field without rewriting the build system for Linyaps. Add the source declaration, Base and Runtime, build dependencies, and launch command, and ensure that every required runtime file enters `${PREFIX}`.
