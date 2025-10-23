# Linyaps Graphics Driver Guide

The Linyaps runtime environment is isolated from the host system environment. When running Linyaps applications that require graphics hardware acceleration, two conditions must be met:

1. The host system must have the graphics drivers properly installed and configured.
2. The corresponding driver packages must be installed in the Linyaps environment.

## Supported Graphics Drivers List

### Mesa Open Source Drivers

The base that applications depend on already includes the appropriate version of Mesa, so no additional installation is required. Mesa open source drivers include:

- amdgpu: The new AMD graphics driver.
- intel: Intel graphics driver.
- nouveau: The open source NVIDIA graphics driver.
- radeon: The legacy AMD/ATI graphics driver.

### Other Drivers

Drivers not included in the base that require additional installation:

- NVIDIA proprietary drivers: Install via `sudo ll-cli install org.deepin.driver.display.nvidia.570-124-04`. The `570-124-04` is the driver version number, which must match the driver version installed on the host system. Check the host driver version through the `/sys/module/nvidia/version` file.
- Glenfly graphics drivers: Install via `sudo ll-cli install com.glenfly.driver.display.arise`.
- Intel video codec drivers (VAAPI): Install via `sudo ll-cli install org.deepin.driver.media.intel`, which includes support for both new and legacy Intel graphics cards.
