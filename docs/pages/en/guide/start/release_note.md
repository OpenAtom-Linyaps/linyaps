# Linyaps Release Notes

---

## Version 1.9

### **New Features**

* **Container Process Management Optimization:** We've integrated **`dumb-init`** as the container's `init` process. This significantly optimizes container internal process management by handling signal forwarding and zombie process cleanup more efficiently.
* **UAB File Generation Refactoring:** The logic for generating UAB files has been completely refactored. All related dependencies are now encapsulated within the **`ll-builder-utils`** toolchain, resolving compatibility issues encountered during export on some distributions.
* **Qt Version Compatibility:** The project now supports both **Qt5 and Qt6**. Applications will automatically compile using the appropriate Qt version, enhancing flexibility.
* **Command-Line Tool Multi-Language Support:** The command-line tool now supports more languages and regional variations, bolstering its internationalization capabilities.
* **Architecture-Specific Configuration Loading:** We've added support for loading architecture-specific configuration files (e.g., `linglong.arm64.yaml`) via **`ll-builder`**, enabling automatic adaptation to different hardware architectures.
* **Build Artifact Compression Algorithm Selection:** The Linglong build tool now allows users to specify the compression algorithm when exporting target artifacts.
* **Local Multi-Repository Support:** We've introduced support for managing multiple local repositories, which can be used for installing and searching applications.

### **Bug Fixes**

* Resolved issues where error messages were vague; they are now clearer and more informative.
* Fixed abnormal behavior in application cache loading and update logic.
* Addressed a defect where update tasks would abnormally terminate during application runtime.
* Rectified an issue causing abnormal errors when uninstalling applications.

---

## Version 1.8

### **New Features**

* **Enhanced Build Capabilities:**
  * **Dependency Management Optimization:** Improved dependency management, now supporting the installation of build toolchain dependencies via the **APT package manager**.
  * **Expanded Compression Algorithms:** When exporting applications, you can now choose from more compression algorithms, including **LZ4, LZMA, and ZSTD**.
  * **Build Environment Upgrade:** The Linglong client now supports building in a **Qt6 environment**.
* **Internationalization Support:** The command-line tool now includes support for additional languages: English (en\_US/en\_GB), Spanish (es), Simplified Chinese (zh\_CN), Catalan (ca), Finnish (fi), Polish (pl), Brazilian Portuguese (pt\_BR), Albanian (sq), and Ukrainian (uk).

### **Bug Fixes**

* **Stability Improvements:**
  * Fixed an issue where mount directories were not fully cleaned up after installing a layer.
  * Resolved failed upgrades for `base` and `runtime` components.
  * Optimized the application uninstallation logic to ensure complete removal of residual directories.
* **Improved Symlink Handling:** Fixed an anomaly where relative path symbolic links were incorrectly converted to empty directories. We also corrected an issue where invalid symbolic links were not properly copied.

---

## Version 1.7

### **New Features**

* **Optimized Repository Storage Layer Structure:** The repository's storage layer structure has been optimized, meaning application management no longer strictly depends on the file system, increasing flexibility.
* **Optimized Linglong Data File Export:** The Linglong data file export function no longer exports all files under the `share` directory, reducing export size.
* **Loongson New World Architecture Support:** Ruyi Linglong now supports application packaging and execution on the **Loongson New World architecture**.
* **Adjusted and Optimized Installation, Uninstallation, and Update Behavior:**
  * The new version no longer supports installing multiple versions of the same application locally. For multiple older versions retained after a client upgrade, all package management operations (except uninstallation) will only apply to the highest version.
  * When an application is upgraded or downgraded while running, the uninstallation of the old version will be delayed to ensure a smooth transition.
* **Internationalization of Ruyi Linglong Command-Line Tool Help Information:** The command-line tool's help information has begun internationalization, currently supporting Chinese, English, and Spanish. We plan to integrate a professional internationalization translation platform in the future.
* **Optimized Linglong Command-Line Argument Parsing:** A new command-line argument parsing framework has been adopted, making parameter information clearer and easier to read.
* **Optimized Linglong Application Packaging and Building:** When using the new build tool to package applications, debugging symbols will be stripped to effectively reduce the final application package size.
* **`runtime/base` Management Command Adjustment:** `runtime/base` no longer supports uninstallation using the `uninstall` command. Instead, the `prune` command is provided for cleaning up unused `runtime` and `base` components.
* **New `ll-cli list --upgradable` Command:** This command is now available to display a list of all upgradable versions for currently installed applications.

### **Bug Fixes**

* Resolved issues preventing successful debugging of Linglong applications.
* Fixed a bug that caused the `ll-cli search` command to crash when searching for applications with a system proxy enabled.

---

## Version 1.6

### **New Features**

* **USB Drive Directory and File Access:** Linglong applications now support direct reading of directories and files from USB drives.
* **New `ll-pica-flatpak` Tool:** We've introduced the **`ll-pica-flatpak`** tool, which supports converting `Flatpak` format applications to Linglong format.

### **Bug Fixes**

* Fixed an issue where scripts failed to execute during Linglong package upgrades.
* Resolved a problem where other commands would be blocked when installing Linglong applications.
* Fixed display issues in the application list when an application name was too long.
