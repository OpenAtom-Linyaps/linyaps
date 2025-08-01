# 如意玲珑发布日志

---

## 1.9 版本

### **新功能**

* **容器进程管理优化：** 引入 **`dumb-init`** 作为容器 `init` 进程，负责转发信号并清理僵尸进程，显著优化了容器内部的进程管理效率。
* **UAB 文件生成重构：** 彻底重构了 UAB 文件的生成逻辑，将所有相关依赖封装到 **`ll-builder-utils`** 工具链中，解决了在部分发行版中导出时的兼容性问题。
* **Qt 版本兼容：** 项目现已同时支持 **Qt5 和 Qt6**，编译应用时将自动选择合适的 Qt 版本，提升了灵活性。
* **命令行工具多语言支持：** 命令行工具已支持更多国家和地区的语言，增强了国际化能力。
* **架构特定配置加载：** 支持通过 **`ll-builder`** 加载架构特定的配置文件（例如 `linglong.arm64.yaml`），实现不同硬件架构的自动适配。
* **构建产物压缩算法选择：** 玲珑构建工具在导出目标产物时，现在允许用户指定压缩算法。
* **本地多仓库支持：** 新增支持本地多仓库管理功能，可用于安装和搜索应用。

### **问题修复**

* 修复了错误信息提示过于模糊的问题，现在提示更加清晰明了。
* 修复了应用缓存加载和更新逻辑中的异常问题。
* 修复了应用运行时，更新任务异常终止的缺陷。
* 修复了卸载应用时可能发生的异常报错问题。

---

## 1.8 版本

### **新功能**

* **构建能力增强：**
  * **依赖管理优化：** 改进了依赖管理机制，支持通过 **APT 包管理器**安装构建工具链的依赖项。
  * **压缩算法扩展：** 应用导出时，现在支持选择更多压缩算法，包括 **LZ4、LZMA 和 ZSTD**。
  * **编译环境升级：** 玲珑客户端现已支持在 **Qt6 环境**下进行构建。
* **国际化支持：** 命令行工具新增对多种语言的支持，包括英语（en_US/en_GB）、西班牙语（es）、简体中文（zh_CN）、加泰罗尼亚语（ca）、芬兰语（fi）、波兰语（pl）、巴西葡萄牙语（pt_BR）、阿尔巴尼亚语（sq）和乌克兰语（uk）。

### **问题修复**

* **稳定性改进：**
  * 修复了安装层（layer）后挂载目录未能完全清理的问题。
  * 解决了基础环境（base）与运行时（runtime）组件升级失败的缺陷。
  * 优化了应用卸载逻辑，确保残留目录能够被彻底清除。
* **符号链接处理机制完善：** 修复了相对路径符号链接被错误地转换为空目录的异常；同时修正了无效符号链接未能被正确复制的问题。

---

## 1.7 版本

### **新功能**

* **仓库存储层结构优化：** 优化了仓库的存储层结构，使得应用管理不再强制依赖文件系统，提升了灵活性。
* **玲珑数据文件导出优化：** 玲珑数据文件导出功能不再导出 `share` 目录下所有文件，减少了导出体积。
* **龙芯新世界架构支持：** 如意玲珑现已支持**龙芯新世界架构**下的应用打包和运行。
* **安装、卸载、更新行为调整优化：**
  * 新版本不再支持在本地同时安装同一应用的多个版本。对于客户端升级后仍保留的多个旧版本，除卸载操作外，所有软件包管理操作将仅对最高版本生效。
  * 当应用处于运行状态时进行升级或降级，旧版本的卸载动作将延迟执行，以确保平滑过渡。
* **如意玲珑命令行工具帮助信息国际化：** 命令行工具的帮助信息已开始国际化，目前支持中文、英文和西班牙语，后续将接入专业的国际化翻译平台。
* **玲珑命令行参数解析优化：** 采用全新的命令行参数解析框架，使参数信息展示更清晰、易读。
* **玲珑应用打包构建优化：** 使用新构建工具打包应用时，将剥离应用调试符号，以有效减小最终应用包的体积。
* **`runtime/base` 管理命令调整：** `runtime/base` 不再支持使用 `uninstall` 命令卸载，转而提供 `prune` 命令用于清理未使用的 `runtime` 和 `base` 组件。
* **新增 `ll-cli list --upgradable` 命令：** 该命令用于显示当前已安装应用程序的所有可更新版本列表。

### **问题修复**

* 解决了玲珑应用调试失败的问题。
* 修复了在开启系统代理后，使用 `ll-cli search` 命令查找应用时可能导致应用崩溃的缺陷。

---

## 1.6 版本

### **新功能**

* **U 盘目录文件读取：** 玲珑应用现已支持直接读取 U 盘中的目录和文件。
* **`ll-pica-flatpak` 工具新增：** 新增了 **`ll-pica-flatpak`** 工具，支持将 `Flatpak` 格式的应用程序转换为玲珑格式。

### **问题修复**

* 修复了升级玲珑包时脚本执行失败的问题。
* 修复了在安装玲珑应用时执行其他命令可能被阻塞的缺陷。
* 修复了应用名称过长时，在查看应用列表时显示不完整的问题。
