<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# Common Build Issues

1. For `cmake` type builds, `-lxxx` fails but both `ldconfig` and `pkg-config` can query the library information.

   The link library path is not in the conventional path, the new path is `/runtime/lib`.

   Add environment variable `LIBRARY_PATH=<libpath>`, the build environment currently includes this environment variable by default.

2. Static library linking fails during build, requiring rebuild with `fPIC`.

   Use the `-fPIC` parameter when building static libraries.

3. Failed to start `box` during build, as shown below:

   ![ll-box startup failure](images/ll-box-start-failed.png)

   The kernel does not support `unprivilege namespace`, enable `unprivilege namespace` to resolve.

   ```bash
   sudo sysctl -w kernel.unprivileged_userns_clone=1
   ```

4. `qtbase` builds successfully, but cannot build `qt` applications, showing `module,mkspec` related errors.

   Low version `fuse-overlay mount` has issues, causing file content corruption during `qtbase commit`, making it unusable. Use `fuse-overlayfs >= 1.7` version.
