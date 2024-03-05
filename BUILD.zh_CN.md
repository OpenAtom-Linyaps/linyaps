# 构建指南

玲珑使用 [cmake 预设]，你可以通过运行以下命令来构建和安装玲珑：

```bash
cmake --workflow --preset release
sudo cmake --install build-release
```

如果你想开发或调试玲珑：

```bash
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
