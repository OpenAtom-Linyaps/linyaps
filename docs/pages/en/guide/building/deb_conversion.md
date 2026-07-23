<!--
SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.

SPDX-License-Identifier: LGPL-3.0-or-later
-->

# Convert from deb Manually

This chapter uses the amd64 deb package of Google Antigravity to show how to unpack a deb in `linglong.yaml`, reorganize the application directory, correct desktop entries, and add runtime dependencies.

This method does not rebuild the application from source. It arranges the prebuilt binaries and resources from the deb as a Linyaps application. Before converting other software, make sure its license permits repackaging and redistribution.

## Example Configuration

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

## Package Information and Base

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

- `package.id` is the unique identifier used to install and run the application and export its desktop files.
- `package.version` is the four-part version number of this converted application.
- `kind: app` identifies the package as a runnable application.
- `base` provides the base system environment required at runtime. This example declares no additional Runtime.

## Declare the deb File

```yaml
sources:
  - kind: file
    url: https://us-central1-apt.pkg.dev/projects/antigravity-auto-updater-dev/pool/antigravity-debian/antigravity_1.11.9-1764120415_amd64_645c2dc91f7e01423b7163dae193052f.deb
    digest: 96bc49a6a2d44cebef29a3592088a0501a517e06e809e0754dee9f3f6286916c
    name: antigravity.deb
```

- `kind: file` downloads a single file without extracting it automatically.
- `url` points to the deb to convert. `amd64` in the file name means that the package targets x86_64.
- `digest` verifies the download and detects changed content.
- `name: antigravity.deb` fixes the downloaded file name, allowing the build script to access it at `/project/linglong/sources/antigravity.deb`.

When updating the deb, update `package.version`, the URL, and the digest together.

## Unpack the deb

```bash
rm -rf antigravity
dpkg-deb -R /project/linglong/sources/antigravity.deb antigravity
```

`rm -rf antigravity` removes the relative directory left by the previous build so stale files cannot affect the result. `dpkg-deb -R` unpacks the deb data and control information into `antigravity`.

The example then copies `/usr/share` from the deb:

```bash
cp -r antigravity/usr/share $PREFIX/
```

After the copy, `/usr/share/antigravity` from the deb becomes `${PREFIX}/share/antigravity`. The application's executable, resources, desktop files, and icons are all placed in the Linyaps application directory.

Do not assume that every deb keeps all application content in `/usr/share`. Inspect the unpacked directory and copy `/usr/bin`, `/usr/lib`, `/opt`, or other required content as appropriate, while ensuring that the final files reside under `${PREFIX}`.

## Correct the desktop Files

The desktop files in the deb use the host path `/usr/share/antigravity/antigravity`. After conversion the application resides in its own Linyaps directory, so update `Exec` in both desktop files:

```bash
sed -i -e 's#Exec=/usr/share/antigravity/antigravity#Exec=/opt/apps/google.antigravity.antigravity/files/share/antigravity/bin/antigravity#' $PREFIX/share/applications/antigravity.desktop
sed -i -e 's#Exec=/usr/share/antigravity/antigravity#Exec=/opt/apps/google.antigravity.antigravity/files/share/antigravity/bin/antigravity#' $PREFIX/share/applications/antigravity-url-handler.desktop
```

The resulting absolute path combines the application ID with the runtime location corresponding to `${PREFIX}/share/antigravity/bin/antigravity`.

Then rename the desktop files with the application ID:

```bash
mv $PREFIX/share/applications/antigravity.desktop $PREFIX/share/applications/google.antigravity.antigravity.desktop
mv $PREFIX/share/applications/antigravity-url-handler.desktop $PREFIX/share/applications/google.antigravity.antigravity-url-handler.desktop
```

Names that contain the application ID reduce the chance of colliding with desktop files from the host or another application and make exported resources easier to identify.

## Install the Icon

```bash
mkdir -p $PREFIX/share/icons/hicolor/512x512/apps
cp $PREFIX/share/pixmaps/antigravity.png $PREFIX/share/icons/hicolor/512x512/apps/
```

These commands create the 512×512 application-icon directory defined by the freedesktop icon-theme convention and copy the icon from the deb. Also confirm that the `Icon` field in the desktop file matches the installed icon name.

## Add Runtime Dependencies

```yaml
buildext:
  apt:
    depends: [libsecret-1-0]
```

`libsecret-1-0` is required at runtime. Packages in `buildext.apt.depends` are included in the final application; dependencies used only during the build should instead be declared in `build_depends`.

For another deb, begin with its dependency metadata and actual runtime errors, but do not copy every system dependency indiscriminately. Components already provided by the Base need not be bundled again.

## Set the Launch Command

Based on the build script and the actual executable path in the desktop files, the launch command is:

```yaml
command: [/opt/apps/google.antigravity.antigravity/files/share/antigravity/bin/antigravity]
```

`command`, `Exec` in the desktop files, and the executable that actually exists after the build must agree. Otherwise, the application may build successfully but fail to start.

## Build and Verify

Run these commands in the directory containing `linglong.yaml`:

```bash
ll-builder build
ll-builder run
```

After the build, check that:

1. `${PREFIX}/share/antigravity/bin/antigravity` exists and is executable.
2. `Exec` in both desktop files points to the Linyaps application directory.
3. `Icon` in the desktop files matches the icon installed in the hicolor directory.
4. The application can access `libsecret-1-0`, start normally, open files, and handle URLs.
5. The package contains no remaining references to the host path `/usr/share/antigravity`.

See [Introduction to the Build Configuration File](./manifests.md) for field descriptions and the [Linyaps Application Packaging Specification](./linyaps_package_spec.md) for directory requirements.
