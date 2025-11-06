# Adapting Qt5-based Open Source Application--qBittorrent to Support Automated Linyaps Application Build Project

After learning from the previous chapter "Compiling Qt5-based Open Source Application--qBittorrent in Linyaps Container", we have roughly understood how to compile the Qt5-based open source application--qBittorrent project source code into executable binary program files in the Linyaps container.

Today, we will complete the final step in the "Basic Steps of Linyaps Application Building" on this basis--writing a complete Linyaps application build project configuration file `linglong.yaml`, mainly achieving the following goals:

1. Automatically pull open source project source code
2. Automatically apply patches to modify the source code
3. Automatically execute compilation, build, and installation operations

## Preliminary Preparation

According to the requirements in "Basic Knowledge of Linyaps Application Build Project" regarding "General Resource Specifications for Linyaps Application Build Project", we should provide both `icons` icon files and `desktop` launch files that ensure desktop user experience for a graphical application.

Therefore, in order to write a complete `linglong.yaml` that can automatically compile `qBittorrent` today, we need to additionally prepare the following materials:

1. Non-binary file general resources: icons, desktop files
2. Main program qBittorrent open source project repository git information, tag version, commit information
3. Third-party runtime library libtorrent open source project repository git information, tag version, commit information

### General Resource Preparation

Since in the previous lesson we successfully compiled and ran `qBittorrent` in the Linyaps container, and this application provided both the icons directory and desktop launch file after installation to $PREFIX, we check these two items and confirm they both comply with the "Freedesktop XDG Specification", so we only need to copy them directly from the container to local, that is, copy to the build directory `/project`.

```
ziggy@linyaps23:/project/src/qBittorrent-release-4.6.7-szbt2/build$ ls $PREFIX/share/applications/
org.qbittorrent.qBittorrent.desktop
```

```
ziggy@linyaps23:/project/src/qBittorrent-release-4.6.7-szbt2/build$ ls $PREFIX/share/icons/hicolor/128x128/apps/
qbittorrent.png
```

Thus, we obtained the "non-binary file general resources". For convenience of being used by build components, I placed these files in the `template_app` subdirectory of the build directory. The current structure is:

```
template_app
├── linglong.yaml
└── template_app
    ├── applications
    │   └── org.qbittorrent.qBittorrent.desktop
    ├── icons
    │   └── hicolor
    │       ├── 128x128
    │       │   ├── apps
    │       │   │   └── qbittorrent.png
    │       │   └── status
    │       │       └── qbittorrent-tray.png
    │       └── scalable
    │           ├── apps
    │           │   └── qbittorrent.svg
    │           └── status
    │               ├── qbittorrent-tray-dark.svg
    │               ├── qbittorrent-tray-light.svg
    │               └── qbittorrent-tray.svg
```

### Desktop Launch File Customization

According to the "General Resource Specifications for Linyaps Application Build Project" in the basic knowledge course, we need to ensure the current desktop file complies with relevant specifications.
We open the desktop file exported from the container, check the Exec and Icon fields, and draw the following conclusions:

```
[Desktop Entry]
Categories=Network;FileTransfer;P2P;Qt;
Exec=qbittorrent %U
GenericName=BitTorrent client
Comment=Download and share files over BitTorrent
Icon=qbittorrent
```

Main conclusions:

1. Icon field value is consistent with icon file, compliant with specification
2. Exec field value is not the compilation result in Linyaps container, needs to be modified to content that complies with "General Resource Specifications for Linyaps Application Build Project". Here we replace it with absolute path pointing to the specific binary file in the container, used to wake up the container and launch the application

## Build Project Configuration File `linglong.yaml` Preparation

After preparing the essential general resources for graphical applications, we begin writing build rules.
Since in the previous lesson we already prepared a simple version of `linglong.yaml` that doesn't have complete build functionality, we can customize it based on that. The current initial state is:

```yaml
version: "4.6.7.2"

package:
  id: org.qbittorrent.qBittorrent
  name: "qBittorrent"
  version: 4.6.7.2
  kind: app
  description: |
    qBittorrent binary

base: org.deepin.foundation/23.0.0
runtime: org.deepin.Runtime/23.0.1

command:
  - /opt/apps/org.qbittorrent.qBittorrent/files/bin/qbittorrent

sources:
  - kind: local
    name: "qBittorrent"

build: |
  mkdir -p ${PREFIX}/bin/ ${PREFIX}/share/
```

### Build Rule Writing & Testing

For smooth transition, I will first import the compilation instructions into the build rules, temporarily not introducing automatic git repository pulling content, to ensure the build rules we write are accurate and usable.
Since it's not recommended to execute too many `tar` instructions in build rules, I open two shell windows in the build directory here, respectively for "Linyaps Container Operations" and "Normal Operations".
The following is the formal transformation process:

1. Through the "Normal Operations" window, use `git` to pull or extract `qBittorrent` and `libtorrent-rasterbar` source code to the build directory. Here I extract them to subdirectories through source code compressed packages separately

```
ziggy@linyaps23:/media/szbt/Data/ll-build/QT/qBittorrent-local$ tar -xvf qBittorrent-4.6.7-git-origin-src.tar.zst -C src/
ziggy@linyaps23:/media/szbt/Data/ll-build/QT/qBittorrent-local$ tar -xvf libtorrent-rasterbar-2.0.9.tar.gz -C 3rd/

```

2. From the "Linyaps Application Directory Structure Specification", we know that the build directory will be mapped as `/project`, so we need to write the manual compilation commands used in the previous lesson into the `build` module

```
build: |
  mkdir -p ${PREFIX}/bin/ ${PREFIX}/share/
  ##Build 3rd libs Comment: Enter `libtorrent-rasterbar` source directory and compile install to container
  mkdir /project/3rd/libtorrent-rasterbar-2.0.9/build
  cd /project/3rd/libtorrent-rasterbar-2.0.9/build
  cmake -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=$PREFIX ..
  make -j$(nproc)
  make install
  ##Build main Comment: Enter `qBittorrent` source directory and compile install to container
  mkdir /project/src/qBittorrent-release-4.6.7-szbt2/build
  cd /project/src/qBittorrent-release-4.6.7-szbt2/build
  cmake -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=$PREFIX ..
  make -j$(nproc)
  make install
  ##Extract common res Comment: Copy general files to install to container corresponding directory, comply with "Linyaps Application Directory Structure Specification"
  cp -rf /project/template_app/* ${PREFIX}/share/
```

After completing this build rule block, we can start trying to compile local source code into binary programs through automated building and export Linyaps application installation package `binary.layer`. \* Since this version of configuration file doesn't provide extract and delete functions, these directories need to be cleared and re-extracted before each rebuild

### Local One-Stop Build Test

After completing the `build` module, the current `linglong.yaml` status is:

```yaml
version: "2"

package:
  id: org.qbittorrent.qBittorrent
  name: "qBittorrent"
  version: 4.6.7.2
  kind: app
  description: |
    qBittorrent binary

base: org.deepin.foundation/23.0.0
runtime: org.deepin.Runtime/23.0.1

command:
  - /opt/apps/org.qbittorrent.qBittorrent/files/bin/qbittorrent

source:
  - kind: local
    name: "qBittorrent"

build: |
  ##Build 3rd libs
  mkdir -p ${PREFIX}/bin/ ${PREFIX}/share/
  mkdir /project/3rd/libtorrent-rasterbar-2.0.9/build
  cd /project/3rd/libtorrent-rasterbar-2.0.9/build
  cmake -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=$PREFIX ..
  make -j$(nproc)
  make install
  ##Build main
  mkdir /project/src/qBittorrent-release-4.6.7-szbt2/build
  cd /project/src/qBittorrent-release-4.6.7-szbt2/build
  cmake -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=$PREFIX ..
  make -j$(nproc)
  make install
  ##Extract common res
  cp -rf /project/template_app/* ${PREFIX}/share/
```

At this point we can return to the build directory and start the build test, execute:

```
ziggy@linyaps23:/media/szbt/Data/ll-build/QT/qBittorrent-local$ ll-builder build -v
```

Thanks to the compilation notes in the Linyaps container, this build quickly ended successfully. We execute the following command to export the container as a Linyaps application installation package `binary.layer`:

```
ziggy@linyaps23:/media/szbt/Data/ll-build/QT/qBittorrent-local$ ll-builder export --layer
```

## Local Build Result Testing

After obtaining the Linyaps application installation package, I tried to experience it on different mainstream distributions that support the Linyaps environment to confirm whether the binary programs built through the Linyaps container have universality.

### deepin 23

![deepin 23](images/3-test-1.png)

### openKylin 2.0

![openKylin 2.0](images/3-test-2.png)

### Ubuntu 2404

![Ubuntu 2404](images/3-test-3.png)

### OpenEuler 2403

![OpenEuler 2403](images/3-test-4.png)

So far, it is sufficient to prove that the "Qt5-based open source application--qBittorrent" can be successfully built and run in third-party distributions that support the "Linyaps" application solution!
