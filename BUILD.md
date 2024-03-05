# Build instructions

linglong use [cmake presets], you can build and install linglong by running:

```bash
cmake --workflow --preset release
sudo cmake --install build-release
```

For developing or debugging linglong:

```bash
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
