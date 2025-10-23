# Linyaps Conversion Tool (pica) Installation

This tool provides the ability to convert deb, appimage, and flatpak packages to Linyaps packages, generating the linglong.yaml file needed to build Linyaps applications, and relies on ll-builder to implement application building and export.

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

Install through [AUR repository](https://aur.archlinux.org/packages/linglong-pica) or [self-built repository](https://github.com/taotieren/aur-repo).

```bash
# AUR
yay -Syu linglong-pica
# Or self-built repository
sudo pacman -Syu linglong-pica
```
