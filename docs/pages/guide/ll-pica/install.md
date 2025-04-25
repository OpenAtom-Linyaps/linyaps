# 玲珑转换工具(pica)安装

本工具提供 deb、appimage、flatpak包转换为如意玲珑包的能力，生成构建如意玲珑应用需要的 linglong.yaml 文件，并依赖 ll-builder 来实现应用构建和导出。

## deepin v23

```bash
sudo apt install linglong-pica
```

## UOS 1070

```bash
echo "deb [trusted=yes] https://ci.deepin.com/repo/deepin/deepin-community/linglong-repo/ unstable main" | sudo tee -a /etc/apt/sources.list
sudo apt update
sudo apt install linglong-pica
```
