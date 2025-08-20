# Linglong conversion tool (pica) installation

This tool provides the ability to convert deb, appimage, and flatpak packages to linyaps packages, generates the linglong.yaml file required to build linyaps applications, and relies on ll-builder to implement application building and export.

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

## Arch Linux

Install via [AUR repository](https://aur.archlinux.org/packages/linglong-pica) or [self-hosted repository](https://github.com/taotieren/aur-repo).

```bash
# AUR
yay -Syu linglong-pica
# OR self-hosted
sudo pacman -Syu linglong-pica
```
