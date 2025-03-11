# 构建指南

在构建linglong之前，确保已经安装以下依赖：

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

玲珑使用 [cmake 预设]，你可以通过运行以下命令来构建和安装玲珑：

```bash
cmake --workflow --preset release
sudo cmake --install build-release
```

如果你想开发或调试玲珑：

```bash
export CMAKE_CXX_COMPILIER_LAUNCHER="$(command -v ccache)"

# 配置，构建然后运行测试
cmake --workflow --preset debug

# 仅配置
cmake --preset debug

# 仅构建
cmake --build --preset debug

# 仅运行测试
ctest --preset debug
```

[cmake 预设]: https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html

## 打包

玲珑使用 [CPM.cmake] 来下载本地找不到的依赖项。

如果你想禁用这个功能：

```bash
export CPM_USE_LOCAL_PACKAGES=1
```

更多信息，请查看 [CPM.cmake] 的 README。

[CPM.cmake]: https://github.com/cpm-cmake/CPM.cmake
