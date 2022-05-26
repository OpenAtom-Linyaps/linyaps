# cmake.yaml

`cmake.yaml` 提供了通用的cmake构建模板, 模板文件如下:

```yaml
variables:
  build_dir: build_dir
  conf_args: |
    -DCMAKE_INSTALL_PREFIX=${PREFIX} \
    -DCMAKE_INSTALL_LIBDIR=${PREFIX}/lib/${TRIPLET}
  extra_args: |
  dest_dir: |
  jobs: -j${JOBS}

build:
  kind: cmake
  manual :
    configure: |
      cmake -B ${build_dir} ${conf_args} ${extra_args}
    build: |
      cmake --build ${build_dir} -- ${jobs}
    install: |
      env DESTDIR=${dest_dir} cmake --build ${build_dir} --target install
```