# Basic Knowledge of Linyaps Application Build Projects

Before officially starting to build a Linyaps application project, we need to have a certain understanding of the basic knowledge regarding Linyaps application building. This will help us better decide what materials to prepare and what types of operations to perform before starting the build project.

## Basic Steps for Linyaps Application Building

Before officially starting to build a Linyaps application project, we need to understand the basic steps a Linyaps application goes through from resource input (source code, binary files, etc.) to application package export. This helps us determine what necessary files we need to prepare:

1. Obtain build target source files (open source project source code, application binary files, etc.)
2. Determine the Linyaps application build type based on source files and select the appropriate build solution
3. Prepare a Linyaps build environment that meets requirements
4. Customize the build configuration file `linglong.yaml` according to the build type and source code content
5. Prepare general resources used by the application, including icons and other non-binary resources

## Materials Required for Linyaps Application Build Projects

Based on the above knowledge, we can understand that throughout the entire Linyaps application build process, the following files are mainly involved:

1. Linyaps application build project configuration file `linglong.yaml`
2. Application source code/binary files and other resources to be packaged
3. General non-binary files and other resources

## Mainstream Standards Followed by Linyaps Applications

In order to ensure complete functionality and good experience, every Linux desktop software package management solution needs to comply with various specifications proposed by the package management solution. This maximizes the functionality of the package management solution and ensures the application ecosystem experience.

Linyaps is not always unique and needs to meet certain specifications to ensure the steady and continuous development of the Linyaps ecosystem.

Currently, the Linyaps solution complies with the following mainstream standards:

1. Freedesktop XDG specification
2. Linyaps application directory structure specification
3. Linyaps application build project configuration file `linglong.yaml` specification

### Freedesktop XDG Specification

1. The Linyaps application solution follows the Freedesktop XDG specification. A normal graphical application should have icon files and desktop files that comply with the Freedesktop XDG specification.
2. Linyaps application icon files should be categorized into different sizes under the `$PREFIX/share/icons/hicolor/` directory
3. Linyaps applications use environment variables like `XDG_DATA_DIRS` in containers to support reading and writing user directories in the host machine

### Linyaps Application Directory Structure Specification

1. Linyaps applications follow the $PREFIX path rule. This variable is automatically generated, and all application-related files need to be stored under this directory. Under this directory level, there are directories like `bin` and `share`.

2. Applications in Linyaps containers will not be allowed to read binary files and runtime libraries in the host machine's system directories

3. During the build process, the build project directory will be mapped to the Linyaps container and mounted as `/project`

4. The directories where runtime libraries and header files are located in Linyaps application containers will vary depending on the runtime environment type:
   - foundation type: Mapped as normal system paths `/usr/bin` `/usr/include` etc. in Linyaps containers, serving as the basic runtime system environment
   - runtime type: Mapped as runtime container paths `/runtime/usr/bin` `/runtime/usr/include` etc. in Linyaps containers, serving as the basic runtime system environment

\*By default, the environment variables inside the Linyaps container have been automatically processed for path recognition, such as:

```
PATH=szbt@szbt-linyaps23:/project$ echo $PATH
/bin:/usr/bin:/runtime/bin:/opt/apps/com.tencent.wechat/files/bin:/usr/local/bin:/usr/bin:/bin:/usr/local/games:/usr/games:/sbin:/usr/sbin
```

General expression is:

```
PATH=szbt@szbt-linyaps23:/project$ echo $PATH
/bin:/usr/bin:/runtime/bin:$PREFIX/bin:/usr/local/bin:/usr/bin:/bin:/usr/local/games:/usr/games:/sbin:/usr/sbin
```

## Specifications for General Resources in Linyaps Application Build Projects

In Linyaps application build projects, different resource files need to comply with relevant specifications to ensure that the build and experience meet requirements

### Icons Directory Specification

According to the `Freedesktop XDG specification` and `Linyaps application directory structure specification` followed by Linyaps, icons are placed in corresponding directories according to different sizes

Mainstream non-vector icon sizes are: `16x16` `24x24` `32x32` `48x48` `128x128` `256x256` `512x512`
To ensure icons can achieve better experience effects in the system, at least one non-vector icon file with a size not smaller than `128x128` is required. Vector icons do not have this restriction.

Therefore, in a Linyaps application installation directory, the icons directory should be as shown in the following example:

```
$PREFIX/share/icons/hicolor/16x16/apps
$PREFIX/share/icons/hicolor/24x24/apps
$PREFIX/share/icons/hicolor/32x32/apps
$PREFIX/share/icons/hicolor/48x48/apps
$PREFIX/share/icons/hicolor/128x128/apps
$PREFIX/share/icons/hicolor/256x256/apps
$PREFIX/share/icons/hicolor/512x512/apps
$PREFIX/share/icons/hicolor/scalable/apps
```

\* The `scalable` directory is used to place `vector icon` files, generally in `.svg` format

Assuming your Linyaps application provides both a non-vector icon file `linyaps-app-demo.png` with size `128x128` and a vector icon file `linyaps-app-demo.svg` with size `128x128`, they should appear in the following state in the Linyaps container:

```
$PREFIX/share/icons/hicolor/128x128/apps/linyaps-app-demo.png
$PREFIX/share/icons/hicolor/scalable/apps/linyaps-app-demo.svg
```

\* To avoid icon conflicts being overwritten, please use the application's `unique English name` or `Linyaps application ID` for the icon file name

### Desktop File Specification

Linyaps applications are compatible with most `desktop startup files` that comply with the `Freedesktop XDG specification`. The following fields need special attention:

| Field | Value Requirements                                                                                                                                                                                                           |
| ----- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Exec  | This value is used to set the command executed when clicking this desktop file. It needs to be consistent with the `command` value in `linglong.yaml`                                                                        |
| Icon  | This value is used to set the application icon displayed by this desktop file. It needs to be consistent with the icon file name in the `Icons Directory Specification`. This value does not require the file name extension |

Therefore, a desktop file that complies with Linyaps application specifications can refer to:

```
org.qbittorrent.qBittorrent.desktop
[Desktop Entry]
Categories=Network;FileTransfer;P2P;Qt;
Exec=/opt/apps/org.qbittorrent.qBittorrent/files/bin/qbittorrent %U
Comment=Download and share files over BitTorrent
Icon=qbittorrent
MimeType=application/x-bittorrent;x-scheme-handler/magnet;
Name=qBittorrent
Type=Application
StartupWMClass=qbittorrent
Keywords=bittorrent;torrent;magnet;download;p2p;

StartupNotify=true
Terminal=false
```

## Linyaps Application Build Project `linglong.yaml` Specification

Like other traditional package management suites, manually creating a Linyaps application build project requires setting up a build rule file `linglong.yaml`. In the build rules, it is divided into `global fields` and `custom fields` according to usage. \* In the case, all space symbols and placeholders in the `linglong.yaml` body are valid characters. Please do not delete or change the format

### Global Field Specification

In `linglong.yaml`, fields that are not affected by the build type are called `global fields`. The following are the main reference specifications:

1. A `linglong.yaml` that can normally start a build project should contain the following key parts:
   | Module | Explanation |
   |--------|-------------|
   | version | Build project version number |
   | package | Basic information of Linyaps application |
   | base | Linyaps application container base environment and version setting. The base environment contains some basic runtime libraries |
   | runtime | Linyaps application runtime library `runtime` and version setting. This module can be deleted when the basic runtime libraries in `base` meet the program runtime requirements |
   | command | Command executed when Linyaps application container starts. It is consistent with the `Exec` field content of the `desktop` file |
   | sources | Source file type of Linyaps application build project |
   | build | Build rules to be executed by Linyaps application build project |

The `package` module contains several sub-modules:
| Sub-module | Explanation |
|-------------|-------------|
| id | Linyaps application ID/package name |
| name | Linyaps application name, use English name |
| version | Linyaps application version number |
| kind | Linyaps application type, default is `app` |
| description | Linyaps application description |

2. Linyaps applications follow the $PREFIX path rule. This variable is automatically generated, and all application-related files need to be stored under this directory. If installation file operations are needed in the build rules, they all need to be installed to the $PREFIX path. \* The $PREFIX variable name is the actual content filled in. **Please do not use `absolute path` or any content with absolute value effect as a substitute**

3. Linyaps applications currently follow the `four-digit` version number naming rule. Applications that do not comply with the rule cannot start the build project

4. `base` and `runtime` versions support automatic matching of the latest version `tail number`. The version number can only fill in the `first three digits` of the version number. For example:
   When base `org.deepin.foundation` provides both `23.0.0.28` and `23.0.0.29`, if only the following is filled in `linglong.yaml`:

```
base: org.deepin.foundation/23.0.0
```

Then when starting the Linyaps application build project, it will default to using the highest version number `23.0.0.29`

5. The Linyaps application build project configuration file is currently not directly compatible with configuration files of other package build tools. It needs to be adapted and modified according to build project configuration file cases:
   https://linglong.dev/guide/ll-builder/manifests.html

### Custom Fields

According to the Linyaps application build project source file type, Linyaps application build projects can be divided into `local file builds` and `git source repository pull builds`. Different types require filling in different `linglong.yaml` content.
The Linyaps application build project source file type `sources` mainly supports these types: `git` `local` `file` `archive`
Complete description reference: [Build Configuration File Introduction](https://linyaps.org.cn/guide/ll-builder/manifests.html)

#### Git Pull Source Code Compilation Mode

When the Linyaps application build project needs to pull open source project repository resources to local for building through git, the `sources` source file type `kind` should be set to `git` and fill in the `linglong.yaml` according to requirements.
At this time, you need to write the `sources` and `build` modules according to specifications.

1. `sources` Example:

```yaml
sources:
  - kind: git
    url: https://githubfast.com/qbittorrent/qBittorrent.git
    version: release-4.6.7
    commit: 839bc696d066aca34ebd994ee1673c4b2d5afd7b

  - kind: git
    url: https://githubfast.com/arvidn/libtorrent.git
    version: v2.0.9
    commit: 4b4003d0fdc09a257a0841ad965b22533ed87a0d
```

| Name    | Description                                                                                                                                                                                                                                                                                                          |
| ------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| kind    | Source file type                                                                                                                                                                                                                                                                                                     |
| url     | Source code repository address that needs to be pulled through git. The repository needs to support git functionality. When the network condition is poor, mirror addresses can be used as alternatives                                                                                                              |
| version | Specify the version number of the source code repository, i.e., `tag label`, or pull the main line `master`                                                                                                                                                                                                          |
| commit  | Pull source code based on the `commit` change history of the repository. Fill in the value corresponding to the commit here, which will apply all changes up to this commit in the repository. \*This field has higher priority than `version`. Please do not fill in any `commit` after the merge time of `version` |

\* Supports adding multiple git repositories as `sources` pulls simultaneously

2. `build` Example:

```yaml
build: |
  mkdir -p ${PREFIX}/bin/ ${PREFIX}/share/
  ##Apply patch for qBittorrent
  cd /project/linglong/sources/qBittorrent.git
  git apply -v /project/patches/linyaps-qBittorrent-4.6.7-szbt2.patch
```

This module is the main body of build rules, and the path follows the `Linyaps Application Directory Structure Specification`
After `sources` is pulled to local, the repository files will be stored in the `/project/linglong/sources` directory. At this time, different repository directories are named with `xxx.git`
Supports using the `git patch` function to conveniently maintain the source code

#### Local Resource Operation Mode

When the Linyaps application build project needs to operate on files in the build directory, the `kind` should be set to `local` type and fill in the `linglong.yaml` according to requirements.
At this time, you need to write the `sources` and `build` modules according to specifications.

1. `sources` Example:

```yaml
sources:
source:
  - kind: local
    name: "qBittorrent"
```

| Name | Description                                    |
| ---- | ---------------------------------------------- |
| kind | Source file type                               |
| name | Source file name identifier, has no actual use |

\* When the `kind` is set to `local` type, the build project will not perform any operations on source files

2. `build` Example:

```yaml
build: |
  ##Build main
  mkdir /project/src/qBittorrent-release-4.6.7-szbt2/build
  cd /project/src/qBittorrent-release-4.6.7-szbt2/build
  cmake -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=$PREFIX ..
  make -j$(nproc)
  make install
```

This module is the main body of build rules, and the path follows the `Linyaps Application Directory Structure Specification`
At this time, the `build` rules support multiple writing methods to simulate manual operations. \* You need to ensure that all steps of this build rule can be executed normally, otherwise the current build task will be interrupted

#### Container Internal Manual Operation Mode

If you plan to directly enter the Linyaps container for manual operations instead of through the build rule file `linglong.yaml`, you should refer to the `Local Resource Operation Mode` to fill in the `linglong.yaml`.

1. The `sources` part is written consistently with the `Local Resource Operation Mode`
2. Since manual operations are used, complete and executable `build` rules are not required. At this time, `linglong.yaml` is used to generate a container that meets the description rather than executing all tasks. For specific operations, please refer to subsequent courses about container internal build file cases
