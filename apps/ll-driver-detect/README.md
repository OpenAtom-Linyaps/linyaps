# ll-driver-detect

`ll-driver-detect` 是如意玲珑（Linyaps）的显卡驱动检测工具，用于自动检测系统中的可用显卡驱动，并为用户提供安装选项。

## 功能概述

- **可扩展的驱动检测**: 采用插件式架构 (`DriverDetectionManager`)，可以轻松扩展以支持多种类型的显卡驱动（目前已支持 NVIDIA）。
- **单例运行保证**: 通过文件锁 (`ApplicationSingleton`) 确保在任何时候只有一个检测进程在运行，避免资源浪费和重复通知。
- **简化的用户配置**: 提供一个简单的配置选项，允许用户选择“不再提醒”，配置被保存在 `~/.config/linglong/driver_detection.json` 中。
- **统一的用户通知**: 当检测到一个或多个可安装的驱动时，通过 DBus 向用户发送一个统一的通知。
- **命令行支持**: 提供多种操作模式，包括全自动检测、仅检查和仅安装。

## 安装

该工具是如意玲珑的内置部分，会随 `linglong-bin` 一同安装。

## 使用方法

### 基本用法

```bash
# 运行驱动检测（默认进行检测并发送通知）
ll-driver-detect

# 查看帮助信息
ll-driver-detect --help
```

### 命令行选项

```
用法: ll-driver-detect [选项]

选项:
  -h,--help         显示帮助信息
  -v,--version      显示版本信息
  -f,--force        强制检测和通知，即使配置为“不再提醒”。
  -c,--check-only   仅检查驱动，不进行安装或通知。
  -i,--install-only 仅安装驱动，不发送通知。
```

### 配置管理

本工具的配置非常简单，存储在 `~/.config/linglong/driver_detection.json`。

```json
{
  "neverRemind": false
}
```

- `neverRemind`: 如果设置为 `true`，`ll-driver-detect` 将不会再发送通知。用户在通知中选择“不再提醒”时，此字段会自动设为 `true`。

## 工作原理

### 检测流程

1. **获取单例锁**: 程序启动时，`ApplicationSingleton` 会尝试在 `$XDG_CONFIG_HOME/linglong/ll-driver-detect.lock`（如果 `XDG_CONFIG_HOME` 未设置，则默认为 `~/.config/linglong/ll-driver-detect.lock`）获取文件锁。如果锁已被另一实例持有，则当前进程会安静地退出。
2. **初始化驱动管理器**: 创建一个 `DriverDetectionManager` 实例。
3. **注册与检测**: 管理器的构造函数会注册所有可用的 `DriverDetector` 子类（例如 `NVIDIADriverDetector`）。随后，管理器调用每个检测器的 `detect()` 方法。
4. **结果聚合**: `detect()` 方法返回驱动信息（如果检测成功），包括驱动标识、版本、玲珑包名和当前安装状态。管理器收集所有成功检测到的驱动信息。
5. **执行操作**:
    - 如果没有检测到任何驱动，程序退出。
    - 默认情况下，它会检查配置文件，并决定是否发送通知。
    - 如果使用 `-c, --check-only`，程序会打印出检测到的驱动并退出。
    - 如果使用 `-i, --install-only`，程序会尝试安装所有检测到的驱动并退出。

### 通知机制

- 在默认模式下，如果检测到驱动并且配置文件中的 `neverRemind` 为 `false`（或使用了 `--force` 选项）：
    1. `DBusNotifier` 会构造一个 DBus 消息。
    2. 该消息被发送到系统的通知服务（`org.freedesktop.Notifications`）。
    3. 通知中会包含“立即安装”和“不再提醒”两个选项。
    4. 如果用户选择“安装”，程序会调用 `ll-cli install` 来安装所有检测到的驱动包。
    5. 如果用户选择“不再提醒”，程序会将 `neverRemind` 设为 `true`并保存到配置文件。

## 开发信息

### 项目结构

```
apps/ll-driver-detect/
├── src/
│   ├── main.cpp                        # 主程序入口
│   ├── application_singleton.h/.cpp    # 应用单例管理器
│   ├── driver_detection_manager.h/.cpp # 驱动检测协调器
│   ├── driver_detector.h/.cpp          # 驱动检测器基类
│   ├── nvidia_driver_detector.h/.cpp   # NVIDIA 驱动检测器实现
│   ├── driver_detection_config.h/.cpp  # 配置管理
│   └── dbus_notifier.h/.cpp            # DBus 通知实现
├── README.md                          # 本文档
└── CMakeLists.txt                     # 构建配置
```

### 核心类

- **`ApplicationSingleton`**: 通过文件锁确保全局只有一个实例在运行。
- **`DriverDetectionManager`**: 负责注册和运行所有具体的驱动检测器。
- **`DriverDetector`**: 驱动检测器的抽象基类，定义了 `detect()` 接口。
- **`NVIDIADriverDetector`**: `DriverDetector` 的一个实现，专门用于检测 NVIDIA 驱动。
- **`DriverDetectionConfigManager`**: 负责加载和保存 `driver_detection.json` 配置文件。
- **`DBusNotifier`**: 封装了通过 DBus 发送通知的逻辑。

### 构建

```bash
# 切换到项目根目录
cd ${LINGYAPS_ROOT}

# 创建构建目录
mkdir build && cd build

# 配置和构建
cmake ..
make ll-driver-detect
```

## 许可证

本项目遵循 LGPL-3.0-or-later 许可证。