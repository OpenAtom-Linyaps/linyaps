# Build instructions

Before building Linglong, ensure that the following dependencies are installed:

- cmake
- debhelper-compat (= 12)
- erofsfuse (>= 1.8.3)
- fuse-overlayfs
- intltool
- libcli11-dev (>= 2.4.1)
- libcurl4-openssl-dev
- libdeflate-dev
- libelf-dev
- libexpected-dev (>= 1.0.0~dfsg-2~bpo10+1)
- libfuse3-dev
- libglib2.0-dev
- libgmock-dev
- libgtest-dev
- liblz4-dev
- liblzma-dev
- libostree-dev
- libpcre2-dev
- libseccomp-dev
- libselinux1-dev
- libssl-dev
- libsystemd-dev
- libyaml-cpp-dev (>= 0.6.2)
- libzstd-dev
- nlohmann-json3-dev (>= 3.5.0)
- pkg-config
- qt5-qmake
- qtbase5-dev
- qtbase5-private-dev
- systemd
- zlib1g-dev
- libcap-dev

linglong use [cmake presets], you can build and install linglong by running:

```bash
cmake --workflow --preset release
sudo cmake --install build-release
```

For developing or debugging linglong:

```bash

# Use ccache and ninja if available.
# You might want add this to the rc file of your shell.
export CMAKE_CXX_COMPILER_LAUNCHER=="$(command -v ccache 2>/dev/null)"
if command -v ninja &>/dev/null; then
        export CMAKE_GENERATOR="Ninja"
fi

# configure, build and test
cmake --workflow --preset debug

# configure only
cmake --preset debug

# build only
cmake --build --preset debug

# test only
ctest --preset debug
```

[cmake presets]: https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html

## Packaging

linglong use [CPM.cmake] to download dependencies not found locally.

For packager want to disable this feature:

```bash
export CPM_USE_LOCAL_PACKAGES=1
```

For further information, check README of [CPM.cmake].

[CPM.cmake]: https://github.com/cpm-cmake/CPM.cmake
