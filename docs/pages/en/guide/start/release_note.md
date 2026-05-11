# Linyaps Release Notes

---

## Version 1.12

### 🚀 **New Features**

* **XDG Desktop Portal Integration:** Enabled xdg-desktop-portal support by default for GTK/Qt applications, ensuring file dialogs in sandboxed environments correctly use the host portal.
* **RISC-V 64 Architecture Support:** Added complete support for RISC-V 64 architecture, including architecture recognition and toolchain triplet generation.
* **Working Directory Option:** Added `--workdir` option to `ll-builder run` and `ll-cli run` commands, allowing specification of the application's working directory inside the container.
* **Shell Completion Support:** Added Zsh and Fish shell auto-completion support for `ll-builder` commands.
* **XDP Integration Control:** Added `--disable-xdp` flag, allowing users to disable xdg-desktop-portal integration.

### 🐞 **Bug Fixes**

* **Container Management:** Fixed errors when entering containers, improved container ID prefix matching functionality, and provided clearer error messages.
* **Qt 6 Compatibility:** Resolved Qt 6 D-Bus metatype registration issues, ensuring runtime stability on distributions like Ubuntu 24.04.
* **Symbolic Link Handling:** Fixed the issue where symbolic links could not be properly resolved when traversing the entries directory, ensuring desktop file discovery works correctly.
* **X11 Display Handling:** Improved parsing of XOrg display environment variables, handling various edge cases for protocol/hostname/display number/screen number.
* **D-Bus Address Parsing:** Implemented specification-compliant D-Bus address parsing, added URL encoding/decoding utilities.

---

## Version 1.11

### 🚀 **New Features**

* **Cross-Architecture Build Support:** Added cross-architecture build and export capabilities, supporting application builds across different CPU architectures.
* **NVIDIA Driver Detection:** Introduced `ll-driver-detect` utility for automatic NVIDIA graphics driver detection and configuration.
* **Device Nodes in Extensions:** Support for configuring device nodes in container extensions, enhancing GPU and hardware device access.
* **XDG Desktop Portal Support:** Integrated XDG Desktop Portal for improved desktop integration of sandboxed applications.
* **Manual Extension Loading:** Support for manual extension loading via command line, enhancing application customization.
* **Repository Mirror Management:** Repository configuration now supports enabling and disabling mirrors to optimize dependency pull speed.
* **Wayland Protocol Support:** Added support for Wayland security-context-v1 protocol, improving security in Wayland environments.
* **Build Export Enhancement:** Build tool now includes `--ref` option for exporting UAB packages by specific references.
* **Force Uninstall Option:** Added the `--force` option to the uninstall command to force the uninstallation of the base environment or runtime.

### 🐞 **Bug Fixes**

* **UAB Export Fixes:** Fixed issues with UAB export and cross-architecture builds, updated build templates.
* **Layer Management:** Fixed incorrect removal of re-installed layers during lazy uninstall.
* **Signal Handling:** Improved container signal handling mechanism for better stability across PID namespaces.
* **Memory Safety:** Fixed multiple memory safety issues, improving runtime stability.
* **Qt Compatibility:** Resolved Qt5/Qt6 compilation and linking issues, enhancing cross-version compatibility.
* **Upgrade Process:** Fixed the issue where multiple versions of an application appear when errors occur during the upgrade process.
* **Timezone Handling:** Support for using TZDIR environment variable to bind the local timezone directory.

---

## Version 1.10

### 🚀 **New Features**

* **GPU Support:** The application runtime now supports graphics processing unit (GPU) capabilities to improve computing performance and rendering efficiency.

* **Container Process Management:** Containerized runtimes have been enhanced to support waiting for child processes to terminate, ensuring resource management and system stability.

* **Repository Mirror Control:** Repository configuration now includes the ability to enable and disable mirroring, allowing users to flexibly control and optimize dependency pull speed.

* **Startup Environment Variables:** The application startup command now supports using the `--env` parameter to set runtime environment variables, facilitating dynamic configuration and debugging.

* **Build Tool Export:** The build tool now includes a `--ref` option, supporting the export of UAB packages by specific references, optimizing the distribution and deployment process.

### 🐞 **Bug Fixes**

* **Desktop Integration:** Fixed a logical error that could cause application icons to disappear from the taskbar after updating the Linglong component.

* **File I/O:** Resolved encoding and parsing issues that caused operations to fail when opening file paths containing special characters.

* **Build File Inclusion:** Fixed an issue where the build tools failed to correctly include and export hidden files when exporting packages.

---

## Version 1.9

### 🚀 **New Features**

* **Container Process Management Optimization:** Introduced **`dumb-init`** as the container `init` process, responsible for signal forwarding and zombie process cleanup, significantly optimizing container internal process management efficiency.
* **UAB File Generation Refactoring:** Completely refactored the UAB file generation logic, encapsulating all related dependencies into the **`ll-builder-utils`** toolchain, resolving compatibility issues during export on some distributions.
* **Qt Version Compatibility:** The project now supports both **Qt5 and Qt6**, automatically selecting the appropriate Qt version when compiling applications, improving flexibility.
* **Command Line Tool Multi-language Support:** Command line tools now support more languages from different countries and regions, enhancing internationalization capabilities.
* **Architecture-specific Configuration Loading:** Supports loading architecture-specific configuration files (e.g., `linglong.arm64.yaml`) through **`ll-builder`**, enabling automatic adaptation for different hardware architectures.
* **Build Product Compression Algorithm Selection:** Linyaps build tools now allow users to specify compression algorithms when exporting target products.
* **Local Multi-repository Support:** Added support for local multi-repository management, which can be used for installing and searching applications.

### 🐞 **Bug Fixes**

* Fixed the issue where error messages were too vague, now the prompts are clearer and more explicit.
* Fixed anomalies in application cache loading and update logic.
* Fixed defects where update tasks would abnormally terminate during application runtime.
* Fixed potential abnormal error reporting issues when uninstalling applications.

---

## Version 1.8

### 🚀 **New Features**

* **Enhanced Build Capabilities:**
  * **Dependency Management Optimization:** Improved dependency management mechanism, supporting installation of build toolchain dependencies through **APT package manager**.
  * **Compression Algorithm Extension:** When exporting applications, now supports more compression algorithms, including **LZ4, LZMA, and ZSTD**.
  * **Build Environment Upgrade:** Linyaps client now supports building in **Qt6 environment**.
* **Internationalization Support:** Command line tools added support for multiple languages, including English (en_US/en_GB), Spanish (es), Simplified Chinese (zh_CN), Catalan (ca), Finnish (fi), Polish (pl), Brazilian Portuguese (pt_BR), Albanian (sq), and Ukrainian (uk).

### 🐞 **Bug Fixes**

* **Stability Improvements:**
  * Fixed the issue where mounted directories were not completely cleaned up after installing layers.
  * Resolved defects in upgrading base environment and runtime components.
  * Optimized application uninstall logic to ensure residual directories can be thoroughly cleaned.
* **Symbolic Link Processing Mechanism Improvement:** Fixed the anomaly where relative path symbolic links were incorrectly converted to empty directories; also corrected the issue where invalid symbolic links were not properly copied.

---

## Version 1.7

### 🚀 **New Features**

* **Repository Storage Layer Structure Optimization:** Optimized the repository's storage layer structure, making application management no longer forcibly dependent on the file system, improving flexibility.
* **Linyaps Data File Export Optimization:** Linyaps data file export function no longer exports all files under the `share` directory, reducing export volume.
* **Loongson New World Architecture Support:** Linyaps now supports application packaging and running on **Loongson New World architecture**.
* **Installation, Uninstallation, and Update Behavior Adjustment and Optimization:**
  * The new version no longer supports installing multiple versions of the same application locally. For multiple old versions retained after client upgrade, all package management operations will only take effect on the highest version except for uninstall operations.
  * When upgrading or downgrading while the application is running, the uninstall action of the old version will be delayed to ensure smooth transition.
* **Linyaps Command Line Tool Help Information Internationalization:** Command line tool help information has begun internationalization, currently supporting Chinese, English, and Spanish, with professional internationalization translation platforms to be integrated later.
* **Linyaps Command Line Parameter Parsing Optimization:** Adopted a new command line parameter parsing framework, making parameter information display clearer and more readable.
* **Linyaps Application Packaging Build Optimization:** When using the new build tool to package applications, application debug symbols will be stripped to effectively reduce the final application package size.
* **`runtime/base` Management Command Adjustment:** `runtime/base` no longer supports uninstallation using the `uninstall` command, instead providing the `prune` command for cleaning up unused `runtime` and `base` components.
* **New `ll-cli list --upgradable` Command:** This command is used to display all upgradeable version lists of currently installed applications.

### 🐞 **Bug Fixes**

* Solved the problem of Linyaps application debugging failure.
* Fixed the defect that could cause application crashes when using the `ll-cli search` command to find applications after enabling system proxy.

---

## Version 1.6

### 🚀 **New Features**

* **USB Drive Directory File Reading:** Linyaps applications now support direct reading of directories and files in USB drives.
* **New `ll-pica-flatpak` Tool:** Added the **`ll-pica-flatpak`** tool, supporting conversion of `Flatpak` format applications to Linyaps format.

### 🐞 **Bug Fixes**

* Fixed the issue where script execution failed when upgrading Linyaps packages.
* Fixed the defect where executing other commands might be blocked when installing Linyaps applications.
* Fixed the problem where application names were too long and displayed incompletely when viewing the application list.
