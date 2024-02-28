<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# 常见构建问题

1. `cmake`类型构建，出现`-lxxx`失败，但`ldconfig`与`pkg-config`均能查询到该库信息。

    链接库路径不在常规路径，新路径为`/runtime/lib`。

    添加环境变量 `LIBRARY_PATH=<libpath>`，目前构建环境已默认包含该环境变量。

2. 构建时`link`静态库失败，要求重新使用`fPIC`构建。

    构建静态库时使用`-fPIC`参数。

3. 构建时启动`box`失败，如下图：

    ![ll-box启动失败](images/ll-box-start-failed.png)

    内核不支持`unprivilege namespace`，开启`unprivilege namespace`解决。

    ```bash
    sudo sysctl -w kernel.unprivileged_userns_clone=1
    ```

4. `qtbase`构建成功，但无法构建`qt`应用，提示`module,mkspec` 相关错误。

    低版本`fuse-overlay mount`存在问题，导致`qtbase commit`时文件内容被污染 ，无法正常使用。使用`fuse-overlayfs >= 1.7`版本。
