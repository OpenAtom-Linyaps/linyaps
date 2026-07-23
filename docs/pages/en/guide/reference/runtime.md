# Introduction to Base and Runtime

In the Linyaps packaging system, a Base provides the fundamental runtime environment and a Runtime provides application-framework support. They form a layered dependency relationship. Choose them correctly to preserve cross-distribution compatibility:

    Base-first principle: First select a Base corresponding to a distribution to which the application has been adapted.
    Runtime inheritance principle: The Runtime must be compatible with the Base.

This document describes Base and Runtime packages to help developers select an appropriate combination.

_Base and Runtime versions follow `major.minor.patch.package`; only the first three components are required when referencing one._

## Base Introduction

A Linyaps Base provides the most fundamental runtime environment, including operating-system components such as glibc, Bash, and basic toolchains. As a containerized minimal system image, it gives applications consistent low-level dependencies across distributions. Bases are generally maintained by the Linyaps project and can be referenced directly without depending on host distribution libraries.

The following stable versions are recommended. Bases not listed here are generally experimental and are not recommended. See the [Base List](#base-list) for details.

<table>
    <thead>
        <td>Base</td>
        <td>Repository</td>
        <td>Status</td>
    </thead>
    <tbody>
        <tr>
            <td>org.deepin.base/25.2.2</td>
            <td>deepin 25</td>
            <td>Maintained</td>
        </tr>
        <tr>
            <td>org.deepin.base/23.1.0</td>
            <td>deepin 23</td>
            <td>Unsupported</td>
        </tr>
        <tr>
            <td>org.deepin.foundation/20.0.0</td>
            <td>UOS 20</td>
            <td>Unsupported</td>
        </tr>
    </tbody>
</table>

## Runtime Introduction

A Linyaps Runtime provides a specific environment required by applications, such as DTK, Wine, or GNOME framework libraries. Developers can choose among prebuilt Runtime images maintained by the project. An application can currently use only one Runtime, so a Runtime may contain several environments: a DTK Runtime includes Qt, for example, while a Wine Runtime includes DTK. _An application may also omit the Runtime._

The following stable Runtimes are officially maintained. Runtimes not listed here are generally experimental and are not recommended. See the [Runtime List](#runtime-list) for details.

<table>
    <thead>
        <td>Runtime</td>
        <td>Compatible Base</td>
        <td>Description</td>
    </thead>
    <tbody>
        <tr>
            <td>org.deepin.runtime.dtk/25.2.2</td>
            <td>org.deepin.base/25.2.2</td>
            <td>Includes DTK6 and Qt6</td>
        </tr>
        <tr>
            <td>org.deepin.runtime.webengine/25.2.2</td>
            <td>org.deepin.base/25.2.2</td>
            <td>Includes DTK6, Qt6, and Qt6 WebEngine</td>
        </tr>
        <tr>
            <td>org.deepin.runtime.dtk/23.1.0</td>
            <td>org.deepin.base/23.1.0</td>
            <td>Includes Qt5 and DTK5</td>
        </tr>
        <tr>
            <td>org.deepin.Runtime/20.0.0</td>
            <td>org.deepin.foundation/20.0.0</td>
            <td>Includes Qt5 and DTK5</td>
        </tr>
    </tbody>
</table>

## Base List

### org.deepin.base/25.2.2

Built from the deepin 25.0.10 repository. Supports x86, arm64, and loong64.

_Run `cat /packages.list` in the container to inspect the package list._

[Binary package list](../../../guide/reference/org.deepin.base_25.2.2_binary.list)

[Develop package list](../../../guide/reference/org.deepin.base_25.2.2_develop.list)

### org.deepin.base/23.1.0

Built from the deepin v23 release repository. Supports x86.

_Run `cat /packages.list` in the container to inspect the package list._

[Develop package list](../../../guide/reference/org.deepin.base_23.1.0_develop.list)

### org.deepin.foundation/20.0.0

Built from the UOS 1070 repository. Supports x86, arm64, and loongarch64.

_Run `cat /var/lib/dpkg/status|grep "^Package: "|sort|awk '{print $2}'` in the container to inspect the package list._

[Develop package list](../../../guide/reference/org.deepin.foundation_20.0.0_develop.list)

## Runtime List

### org.deepin.runtime.dtk/25.2.2

Built from the deepin 25.0.10 repository. Supports x86, arm64, loong64, sw64, and mips64.

_Run `cat /runtime/packages.list` in the container to inspect the package list._

[Package list](../../../guide/reference/org.deepin.runtime.dtk_25.2.2_develop.list)

### org.deepin.runtime.webengine/25.2.2

Built from the deepin 25.0.10 repository. Supports x86, arm64, loong64, sw64, and mips64.

_Run `cat /runtime/packages.list` in the container to inspect the package list._

[Package list](../../../guide/reference/org.deepin.runtime.webengine_25.2.2_develop.list)

### org.deepin.runtime.dtk/23.1.0

Built from the deepin 23 release repository. Supports x86.

_Run `cat /runtime/packages.list` in the container to inspect the package list._

[Package list](../../../guide/reference/org.deepin.runtime.dtk_23.1.0_develop.list)

### org.deepin.Runtime/20.0.0

Built from the UOS 1070 repository. Supports x86, arm64, and loongarch64.

_Run `cat /runtime/packages.list` in the container to inspect the package list._

[Package list](../../../guide/reference/org.deepin.Runtime_20.0.0_develop.list)
