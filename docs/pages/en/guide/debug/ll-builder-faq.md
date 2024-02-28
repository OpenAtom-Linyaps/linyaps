<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# Build FAQ

1. `-lxxx` failed in `cmake` type build, but `ldconfig` and `pkg-config` can query the library information.

   The link library path is not a regular path, the new path is `/runtime/lib`. Add the environment variable `LIBRARY_PATH=<libpath>`, which is included in the build environment by default.

2. `link` static library failed when building, which requires re-build with `fPIC`.

   Use the `-fPIC` parameter when building a static library.

3. Failed to start `box` during build, as shown below.

   ![ll-box start failed](images/ll-box-start-failed.png)

   The kernel does not support `unprivilege namespace`, please open `unprivilege namespace` to solve it.

   ```bash
   sudo sysctl -w kernel.unprivileged_userns_clone=1
   ```

4. The build of `qtbase` is successful, but the `qt` application cannot be built, which prompts `module,mkspec` related errors.

   There is a problem in the lower version of `fuse-overlay mount`, which leads to the polluted file content when `qtbase commit`, and cannot be used. Use `fuse-overlayfs >= 1.8.2` version.
