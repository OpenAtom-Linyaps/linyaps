# ll-ctk-detect

`ll-ctk-detect` 是如意玲珑（Linyaps）的容器工具检测工具。

`ll-cli run` 启动应用时会在后台拉起本工具。工具先通过内核模块探测判断显卡设备是否在场，再检查该设备对应的容器工具包是否已安装；如果设备已检测到但工具包缺失，会先通过通知询问用户是否安装。用户确认后，工具才会把 deb 下载到用户私有临时目录并调用安装器安装。

## 命令行

```bash
# 检查当前系统缺失哪些容器工具，不触发下载/安装/通知
ll-ctk-detect --check-only
```

## 配置

默认系统配置：

```text
/etc/linglong/ll-ctk-detect.json
```

用户配置：

```text
~/.config/linglong/ll-ctk-detect.json
```

用户配置优先级高于系统配置，工具列表按 `deviceId` 合并：用户配置中的条目会覆盖同设备系统条目，空 `tools` 不会清空系统工具。用户配置还保存“不再提醒”状态。

配置示例：

```json
{
  "tools": [
    {
      "deviceId": "nvidia",
      "packages": [
        "nvidia-container-toolkit-base"
      ]
    }
  ],
  "neverRemind": {
    "nvidia": false
  }
}
```
