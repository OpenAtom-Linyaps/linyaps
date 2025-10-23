# 如意玲珑显卡驱动说明

如意玲珑运行环境是与宿主机环境隔离的，运行如意玲珑程序时如果需要使用显卡硬件加速功能，需要满足两个条件：

1. 宿主机正确安装、配置了显卡驱动。
2. 在如意玲珑环境中安装对应的驱动包。

## 已适配的显卡驱动列表

### Mesa 开源驱动

在应用依赖的 base 中已经携带了对应版本的 mesa，不需要额外的安装。mesa 开源驱动包括：

- amdgpu，新的 AMD 显卡驱动。
- intel，Intel 显卡驱动。
- nouveau，开源的 nvidia 显卡驱动。
- radeon，老的 AMD/ATI 显卡驱动。

### 其他驱动

不在 base 中携带的，需要额外安装的驱动：

- 英伟达闭源驱动，通过 `sudo ll-cli install org.deepin.driver.display.nvidia.570-124-04` 安装。其中 `570-124-04` 是驱动版本号，需要与宿主机安装的驱动版本匹配，通过宿主机 `/sys/module/nvidia/version` 文件查看宿主机驱动的版本。
- 格兰菲显卡驱动，通过 `sudo ll-cli install com.glenfly.driver.display.arise` 安装。
- 英特尔视频编解码驱动（VAAPI），通过 `sudo ll-cli install org.deepin.driver.media.intel` 安装，包含了新/旧 Intel 显卡的支持。
