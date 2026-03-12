# base 和 runtime 介绍

在玲珑应用打包体系中，base 提供基础运行环境，runtime 提供应用框架支持，两者采用分层依赖关系。正确选型可确保应用跨发行版的兼容性，建议遵循以下选型原则：

    ‌base 优先原则‌：先选择应用已适配的发行版对应的base
    ‌runtime 继承原则‌：runtime必须和base保持兼容

本文档将详细介绍 base 和 runtime，以帮助开发者选择合适的 base 和 runtime。

_base 和 runtime 版本号遵循“主版本.次版本.修订版本.打包版本”的规范，使用时只需填写前三位版本即可_

## base 介绍

玲珑的 base 提供最基础的运行环境，包括操作系统核心组件（如 glibc、bash 等基础工具链）‌。
作为容器化的"最小系统镜像"，确保应用能在不同发行版上保持一致的底层依赖。base 通常由玲珑官方维护，开发者可直接引用而无需担心应用运行在不同发行版上的兼容性问题。

推荐使用以下稳定版本，未在文档中列出的 base 一般是实验性质的版本，不建议选用。更详细的信息见 [base 列表](#base-列表)

<table>
    <thead>
        <td>base</td>
        <td>仓库</td>
        <td>状态</td>
    </thead>
    <tbody>
        <tr>
            <td>org.deepin.base/25.2.2</td>
            <td>deepin 25</td>
            <td>维护阶段</td>
        </tr>
        <tr>
            <td>org.deepin.base/23.1.0</td>
            <td>deepin 23</td>
            <td>停止支持</td>
        </tr>
        <tr>
            <td>org.deepin.foundation/20.0.0</td>
            <td>uos 20</td>
            <td>停止支持</td>
        </tr>
    </tbody>
</table>

## runtime 介绍

玲珑的 runtime 提供应用所需的特定运行环境（如 DTK/Wine/GNOME 等框架库），开发者可以根据应用需求选择合适的 runtime，玲珑官方也提供了多种预编译好的 runtime 镜像供开发者选择。目前应用只能选择一个 runtime 使用，因此 runtime 可能会包含多个环境，例如 DTK 的 runtime 包含 Qt 框架，Wine 的 runtime 包含 DTK 框架。_应用也可以不使用 runtime_

官方维护的稳定 runtime 如下，未在文档中列出的 runtime 一般是实验性质的版本，不建议选用。更详细的信息见 [runtime 列表](#runtime-列表)

<table>
    <thead>
        <td>runtime</td>
        <td>兼容的base</td>
        <td>介绍</td>
    </thead>
    <tbody>
        <tr>
            <td>org.deepin.runtime.dtk/25.2.2</td>
            <td>org.deepin.base/25.2.2</td>
            <td>包含DTK6、Qt6</td>
        </tr>
        <tr>
            <td>org.deepin.runtime.webengine/25.2.2</td>
            <td>org.deepin.base/25.2.2</td>
            <td>包含DTK6、Qt6、Qt6 webengine</td>
        </tr>
        <tr>
            <td>org.deepin.runtime.dtk/23.1.0</td>
            <td>org.deepin.base/23.1.0</td>
            <td>包含Qt5和DTK5</td>
        </tr>
        <tr>
            <td>org.deepin.Runtime/20.0.0</td>
            <td>org.deepin.foundation/20.0.0</td>
            <td>包含Qt5和DTK5</td>
        </tr>
    </tbody>
</table>

## base 列表

### org.deepin.base/25.2.2

基于 deepin 25.0.10 仓库制作的 base，支持 x86, arm64, loong64 架构。

_在容器中执行 `cat /packages.list` 查看包列表_

[binary包列表](./org.deepin.base_25.2.2_binary.list)

[develop包列表](./org.deepin.base_25.2.2_develop.list)

### org.deepin.base/23.1.0

基于 deepin v23 release 仓库制作的 base，支持 x86 架构。

_在容器中执行 `cat /packages.list` 查看包列表_

[develop包列表](./org.deepin.base_23.1.0_develop.list)

### org.deepin.foundation/20.0.0

使用 uos 1070 仓库制作的 base，支持 x86, arm64, loongarch64 三个架构。

_在容器中执行 `cat /var/lib/dpkg/status|grep "^Package: "|sort|awk '{print $2}'` 查看包列表_

[develop包列表](./org.deepin.foundation_20.0.0_develop.list)

## runtime 列表

### org.deepin.runtime.dtk/25.2.2

使用 deepin 25.0.10 仓库制作的 runtime，支持 x86, arm64, loong64, sw64, mips64 架构。

_在容器中执行 `cat /runtime/packages.list` 查看包列表_

[包列表](./org.deepin.runtime.dtk_25.2.2_develop.list)

### org.deepin.runtime.webengine/25.2.2

使用 deepin 25.0.10 仓库制作的 runtime，支持 x86, arm64, loong64, sw64, mips64 架构。

_在容器中执行 `cat /runtime/packages.list` 查看包列表_

[包列表](./org.deepin.runtime.webengine_25.2.2_develop.list)

### org.deepin.runtime.dtk/23.1.0

使用 deepin 23 release 仓库制作的 runtime，支持 x86 架构。

_在容器中执行 `cat /runtime/packages.list` 查看包列表_

[包列表](./org.deepin.runtime.dtk_23.1.0_develop.list)

### org.deepin.Runtime/20.0.0

使用 uos 1070 仓库制作的 runtime，支持 x86, arm64, loongarch64 三个架构。

_在容器中执行 `cat /runtime/packages.list` 查看包列表_

[包列表](./org.deepin.Runtime_20.0.0_develop.list)
