# autotools.yaml

`autotools.yaml` 提供了通用的`automake`类型构建模板, 模板文件如下:

```yaml
variables:
  build_dir: build_dir
  conf_args: |
    --prefix=${PREFIX} \
    --libdir=${PREFIX}/lib/${TRIPLET}
  extra_args: |
  dest_dir: |
  jobs: -j${JOBS}

build:
  kind: autotools
  manual:
    configure: |
      #autogon.sh, bootstrap.sh
      autoreconf -ivf
      ./configure ${conf_args} ${extra_args}
    build: |
      make ${jobs}
    install: |
      make ${jobs} DESTDIR=${dest_dir} install
```