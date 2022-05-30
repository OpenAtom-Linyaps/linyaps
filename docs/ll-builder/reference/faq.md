# ll-builder 构建问题

#### 1. `cmake`类型构建，出现`-lxxx`失败，但`ldconfig`与`pkg-config`均能查询到该库信息
链接库路径不在常规路径，新路径为`/runtime/lib`。添加环境变量 `LIBRARY_PATH=<libpath>`，目前构建环境已默认包含该环境变量。

#### 2. 构建时`link`静态库失败，要求重新使用`fPIC`构建

构建静态库时使用`-fPIC`参数

#### 3. 构建时启动`box`失败，如下图

![ll-box启动失败](../images/ll-box-start-failed.png)

内核不支持`unprivilege namespace`，开启`unprivilege namespace`解决。

```bash
sudo sysctl -w kernel.unprivileged_userns_clone=1
```

#### 3. `qtbase`构建成功，但无法构建`qt`应用，提示`module,mkspec` 相关错误

低版本`fuse-overlay mount`存在问题，导致`qtbase commit`时文件内容被污染 ，无法正常使用。使用`fuse-overlayfs >= 1.8.2`版本。

#### 4. `error: Writing content object: fchown: Operation not permitted`

`ostree`仓库模式不正确，本地`ostree`初始化时，使用`bare-user-only`模式。


#### 5. `cmake check module` 使用`import target`时，`pkgConfig`变量展开会出现`link`时为空值

TODO
