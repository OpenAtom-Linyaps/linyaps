<!--
SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# 手动从 deb 转换

本章以 Google Antigravity 的 amd64 deb 软件包为例，介绍如何在 `linglong.yaml` 中解包 deb、重新组织应用目录、修正桌面入口并补充运行依赖。

该方式不会重新编译应用源码，而是将 deb 中已经编译好的二进制文件和资源整理成如意玲珑应用。转换其他软件前，请确认软件许可证允许重新打包和分发。

## 示例配置

```yaml
version: "1"

package:
  id: google.antigravity.antigravity
  name: antigravity
  version: 1.11.9.0
  kind: app
  description: |
    Google Antigravity - Experience liftoff with the next-generation IDE

command: [/opt/apps/google.antigravity.antigravity/files/share/antigravity/bin/antigravity]

base: org.deepin.base/25.2.1

sources:
  - kind: file
    url: https://us-central1-apt.pkg.dev/projects/antigravity-auto-updater-dev/pool/antigravity-debian/antigravity_1.11.9-1764120415_amd64_645c2dc91f7e01423b7163dae193052f.deb
    digest: 96bc49a6a2d44cebef29a3592088a0501a517e06e809e0754dee9f3f6286916c
    name: antigravity.deb

build: |
  echo 'building...'

  rm -rf antigravity
  dpkg-deb -R /project/linglong/sources/antigravity.deb antigravity

  cp -r antigravity/usr/share $PREFIX/

  # desktop
  sed -i -e 's#Exec=/usr/share/antigravity/antigravity#Exec=/opt/apps/google.antigravity.antigravity/files/share/antigravity/bin/antigravity#' $PREFIX/share/applications/antigravity.desktop
  sed -i -e 's#Exec=/usr/share/antigravity/antigravity#Exec=/opt/apps/google.antigravity.antigravity/files/share/antigravity/bin/antigravity#' $PREFIX/share/applications/antigravity-url-handler.desktop

  mv $PREFIX/share/applications/antigravity.desktop $PREFIX/share/applications/google.antigravity.antigravity.desktop
  mv $PREFIX/share/applications/antigravity-url-handler.desktop $PREFIX/share/applications/google.antigravity.antigravity-url-handler.desktop

  # icon
  mkdir -p $PREFIX/share/icons/hicolor/512x512/apps
  cp $PREFIX/share/pixmaps/antigravity.png $PREFIX/share/icons/hicolor/512x512/apps/

  echo 'build done'

buildext:
  apt:
    depends: [libsecret-1-0]
```

## 软件包信息和 Base

```yaml
package:
  id: google.antigravity.antigravity
  name: antigravity
  version: 1.11.9.0
  kind: app
  description: |
    Google Antigravity - Experience liftoff with the next-generation IDE

base: org.deepin.base/25.2.1
```

- `package.id` 是安装、运行以及导出桌面文件时使用的唯一标识。
- `package.version` 使用四段式版本号表示本次转换的应用版本。
- `kind: app` 表示该软件包是可运行应用。
- `base` 提供应用运行所需的基础系统环境。本示例没有声明额外 Runtime。

## 声明 deb 文件

```yaml
sources:
  - kind: file
    url: https://us-central1-apt.pkg.dev/projects/antigravity-auto-updater-dev/pool/antigravity-debian/antigravity_1.11.9-1764120415_amd64_645c2dc91f7e01423b7163dae193052f.deb
    digest: 96bc49a6a2d44cebef29a3592088a0501a517e06e809e0754dee9f3f6286916c
    name: antigravity.deb
```

- `kind: file` 表示下载单个文件，不对其自动解压。
- `url` 指向需要转换的 deb。文件名中的 `amd64` 表示该软件包面向 x86_64 架构。
- `digest` 用于校验下载文件，防止内容发生变化。
- `name: antigravity.deb` 将下载后的文件名固定下来，因此构建脚本可以通过 `/project/linglong/sources/antigravity.deb` 访问它。

更新 deb 版本时，需要同步更新 `package.version`、URL 和摘要。

## 解包 deb

```bash
rm -rf antigravity
dpkg-deb -R /project/linglong/sources/antigravity.deb antigravity
```

`rm -rf antigravity` 清理上一次构建留下的相对目录，避免旧文件影响结果。`dpkg-deb -R` 将 deb 的数据和控制信息解包到 `antigravity` 目录。

该示例随后复制 deb 中的 `/usr/share`：

```bash
cp -r antigravity/usr/share $PREFIX/
```

复制完成后，deb 中的 `/usr/share/antigravity` 会变成 `${PREFIX}/share/antigravity`。应用的可执行文件、资源、desktop 文件和图标都随之进入如意玲珑应用目录。

转换其他 deb 时，不能假设应用内容都在 `/usr/share`。应先检查解包后的目录，按实际情况复制 `/usr/bin`、`/usr/lib`、`/opt` 或其他运行所需内容，并保持最终文件位于 `${PREFIX}`。

## 修正 desktop 文件

deb 中的 desktop 文件使用宿主机路径 `/usr/share/antigravity/antigravity`。转换后，应用位于如意玲珑自己的目录，因此需要修改两个 desktop 文件中的 `Exec`：

```bash
sed -i -e 's#Exec=/usr/share/antigravity/antigravity#Exec=/opt/apps/google.antigravity.antigravity/files/share/antigravity/bin/antigravity#' $PREFIX/share/applications/antigravity.desktop
sed -i -e 's#Exec=/usr/share/antigravity/antigravity#Exec=/opt/apps/google.antigravity.antigravity/files/share/antigravity/bin/antigravity#' $PREFIX/share/applications/antigravity-url-handler.desktop
```

修改后的绝对路径由应用 ID 和 `${PREFIX}/share/antigravity/bin/antigravity` 对应的运行时位置组成。

随后使用应用 ID 重命名 desktop 文件：

```bash
mv $PREFIX/share/applications/antigravity.desktop $PREFIX/share/applications/google.antigravity.antigravity.desktop
mv $PREFIX/share/applications/antigravity-url-handler.desktop $PREFIX/share/applications/google.antigravity.antigravity-url-handler.desktop
```

使用包含应用 ID 的文件名可以降低与宿主机或其他应用的 desktop 文件发生命名冲突的可能，并使导出资源更容易识别。

## 安装图标

```bash
mkdir -p $PREFIX/share/icons/hicolor/512x512/apps
cp $PREFIX/share/pixmaps/antigravity.png $PREFIX/share/icons/hicolor/512x512/apps/
```

这两条命令创建 freedesktop 图标主题约定的 512×512 应用图标目录，并把 deb 中的图标复制进去。还需要确认 desktop 文件的 `Icon` 字段与安装后的图标名称一致。

## 补充运行依赖

```yaml
buildext:
  apt:
    depends: [libsecret-1-0]
```

`libsecret-1-0` 是应用运行时需要的依赖。`buildext.apt.depends` 中的包会包含到最终应用中；如果只是构建阶段使用，应改为声明在 `build_depends` 中。

转换其他 deb 时，可以从 deb 的依赖信息和实际运行错误开始排查，但不应直接复制所有系统依赖。Base 已经提供的组件不需要重复携带。

## 设置启动命令

根据构建脚本和 desktop 文件中的实际可执行文件位置，启动命令为：

```yaml
command: [/opt/apps/google.antigravity.antigravity/files/share/antigravity/bin/antigravity]
```

`command`、desktop 文件的 `Exec` 和构建后实际存在的可执行文件必须相互一致，否则应用可能构建成功但无法启动。

## 构建和验证

在 `linglong.yaml` 所在目录执行：

```bash
ll-builder build
ll-builder run
```

构建完成后重点检查：

1. `${PREFIX}/share/antigravity/bin/antigravity` 已存在并具有执行权限。
2. 两个 desktop 文件的 `Exec` 均指向如意玲珑应用目录。
3. desktop 文件的 `Icon` 与安装到 hicolor 目录的图标名称一致。
4. 应用能够访问 `libsecret-1-0`，并可以正常启动、打开文件和处理 URL。
5. 包内不存在对宿主机 `/usr/share/antigravity` 的残留引用。

字段说明见[构建配置文件简介](./manifests.md)，目录要求见[玲珑应用打包规范](./linyaps_package_spec.md)。
