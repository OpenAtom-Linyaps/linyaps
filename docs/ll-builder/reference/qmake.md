# qmake.yaml

`qmake.yaml` 提供了通用的qmake构建模板, 模板文件如下:

```yaml
variables:
  build_dir: build_dir
  conf_args: |
    PREFIX=${PREFIX} \
    LIB_INSTALL_DIR=${PREFIX}/lib/${TRIPLET}
  extra_args: |
  dest_dir: |
  jobs: -j${JOBS}

build:
  kind: qmake
  manual :
    configure: |
      qmake -makefile ${conf_args} ${extra_args}
    build: |
      make ${jobs}
    install: |
      make ${jobs} DESTDIR=${dest_dir} install
```