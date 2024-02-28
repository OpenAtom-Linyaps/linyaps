<!--
SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# 调试玲珑应用

以下教程以“构建工具”一节中提到的 [linglong-builder-demo](https://github.com/linuxdeepin/linglong-builder-demo) 项目为例。我们将项目放在 `/path/to/project`。参考教程操作时**注意对路径进行替换**。

由于玲珑应用运行在容器中，想要在宿主机上对其进行调试，我们需要在容器中使用 `gdbserver` 运行应用，所以首先需要安装 `gdbserver`。

我们可以使用发行版提供的 `gdbserver`，以 `apt` 为例：

```bash
sudo apt install gdbserver gdb -y
```

接下来参考“运行编译后的应用”中的教程，通过 `ll-build run` 命令在容器中运行 `bash`，并通过 `gdbserver` 运行需要被调试的应用：

```bash
ll-build run --exec /bin/bash
# 在容器中：
gdbserver :10240 deepin-draw
```

上述命令中，`:10240`为任意当前没有被占用的 tcp 端口。之后我们还需要做两件事情：

1. 在容器外使用 `gdb` 连接上容器中的 `gdbserver`；
2. 设置源码映射路径。

## 在终端中使用 gdb 进行调试

1. 找到容器中被执行的可执行文件在宿主机上的位置，对于如上项目，该文件位于 `/path/to/project/.linglong-target/overlayfs/up/opt/apps/org.deepin.demo/files/bin/demo`。

   对于通过 `ll-cli` 安装的应用，其可执行程序一般位于 `$LINGLONG_ROOT/layers/[appid]/[version]/[arch]/files/bin` 下。

   在容器外使用 `gdb` 加载该程序：

   ```bash
   gdb  /path/to/project/.linglong-target/overlayfs/up/opt/apps/org.deepin.demo/files/bin/demo
   ```

2. 在 `gdb` 中使用 `target` 命令连接 `gdbserver`，操作如下所示：

   ```bash
   target remote :10240
   ```

3. 在 `gdb` 中输入以下命令以设置路径映射，帮助 `gdb` 找到对应的源代码，假定源码放置在主机的`Desktop`，命令如下：

   ```bash
   set substitute-path /source /path/to/project
   ```

之后正常使用 `gdb` 即可。

## QtCreator 配置

参考上述过程，我们可以很轻松地完成 `QtCreator` 的配置：

依次点击：调试>开始调试>连接到正在运行的调试服务器，在对话框中填入：

```text
服务器端口：`10240`

本地执行档案：`/path/to/project/.linglong-target/overlayfs/up/opt/apps/org.deepin.demo/files/bin/demo`

Init Commands: `set substitute-path /source /path/to/project`
```

大致配置如下图所示：

![qt-creator](images/qt-creator.png)

配置完成后，即可正常使用`QtCreator`来进行调试了。
