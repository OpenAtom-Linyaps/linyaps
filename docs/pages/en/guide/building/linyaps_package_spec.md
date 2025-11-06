# Linyaps Application Packaging Specification

The keywords **MUST**, **MUST NOT**, **REQUIRED**, **SHALL**, **SHALL NOT**, **RECOMMENDED**, **MAY**, and **OPTIONAL**[^rfc2119-keywords] in this document shall be interpreted as described in [RFC 2119][rfc-2119].

The correspondence between these keywords and their English counterparts is shown in the following table:

| Chinese    | English     |
| ---------- | ----------- |
| **必须**   | MUST        |
| **禁止**   | MUST NOT    |
| **必要的** | REQUIRED    |
| **应当**   | SHALL       |
| **不应**   | SHALL NOT   |
| **推荐的** | RECOMMENDED |
| **允许**   | MAY         |
| **可选的** | OPTIONAL    |

[rfc-2119]: https://datatracker.ietf.org/doc/html/rfc2119

This document aims to help application developers standardize application building processes to migrate to the Linyaps package management system.

## General

This section mainly records some general specifications that almost all projects should follow.

### Application Name

Application names **SHALL** use [reverse domain name notation](https://en.wikipedia.org/wiki/Reverse_domain_name_notation), such as `org.deepin.demo`.

This application name is mainly used for desktop file naming, DBus service naming, etc.

- Developers **SHALL** use the reverse of a domain they actually own as the application name prefix, followed by the application name

  **Note**: If a developer cannot prove they actually own the domain, it may result in the application package being removed from the repository.

- For third-party applications developed on GitHub, if the organization hosting the application has an additional domain, it **SHALL** be preferred; otherwise, `io.github.<GITHUB_ID>.` **SHALL** be used as the prefix.

  Particularly, if the application's organization name and application name are the same, such as <https://github.com/neovim/neovim>, packagers **SHALL NOT** omit the repeated application name and organization name. The application ID **SHALL** be `io.github.neovim.neovim`.

  **Note**: In reality, the organization owns the domain `neovim.io`, so the most reasonable application name **SHALL** be `io.neovim.neovim`.

- Application names containing `-` are **NOT RECOMMENDED**. If a domain/application name genuinely contains `-`, using `_` **IS RECOMMENDED** instead.

- Application names ending with `.desktop` are **NOT RECOMMENDED**

These specifications come from the [Desktop Entry Specification][desktop-entry-specification].

[desktop-entry-specification]: https://specifications.freedesktop.org/desktop-entry-spec/latest

Further reading: <https://docs.flatpak.org/en/latest/conventions.html#application-ids>

### `prefix` and `$DESTDIR`

When writing an application's build process, developers **SHALL NOT** assume a fixed installation location. Installing executables to hardcoded paths in Makefile/CMakeLists.txt, such as `/usr/bin`, is non-compliant behavior.

When developers write build/install processes, they **SHALL** respect the build system's `prefix` configuration and the value of the `$DESTDIR` environment variable to allow packagers to conveniently configure the application's specific installation location during packaging.

`prefix` refers to the specific location in the system where the application will ultimately be installed, as specified to the build system during build/install time.

When developers do not specify an installation location, its default value **SHALL** be `/usr/local`. When packaging through the package management system, the package management system configures this value. When using dpkg-related tools for packaging, the value is configured as `/usr`. However, when writing build/install processes, developers should consider that `prefix` may be configured to any value.

`$DESTDIR` is an environment variable agreed upon for the build system's installation process to facilitate distribution packaging and other processes. Its general working logic is as follows:

If the build system completes the build work and during the installation process is specified `prefix=/usr` and `$DESTDIR=./tmp`, after completing installation, all product files should appear in the `./tmp/usr` directory. Packaging tools will then treat `./tmp` as the root directory for compression, packaging, and other work on the files contained within.

Here we use common build systems as examples to show **RECOMMENDED** ways of writing common file installation processes. These writing methods all respect the `prefix` configuration and `$DESTDIR`:

#### Makefile

This section is mainly referenced from content in <https://www.gnu.org/prep/standards/standards.html> and <https://wiki.debian.org/Multiarch/Implementation>.

Here we define default values for some variables and other common parts for writing examples later. Explanations of these variable defaults can be found in the links above.

```makefile
DESTDIR ?=

prefix      ?= /usr/local
bindir      ?= $(prefix)/bin
libdir      ?= $(prefix)/lib
libexecdir  ?= $(prefix)/libexec
datarootdir ?= $(prefix)/share

INSTALL         ?= install
INSTALL_PROGRAM ?= $(INSTALL)
INSTALL_DATA    ?= $(INSTALL) -m 644

# The above defined variables with default values are recommended behaviors in the above specifications and should not be modified.

PKG_CONFIG ?= pkg-config

.PHONY: install
```

- Executable files

  ```makefile
  install:
      $(INSTALL) -d "$(DESTDIR)$(bindir)"
      $(INSTALL_PROGRAM) path/to/your/executable "$(DESTDIR)$(bindir)"/executable
  ```

- Internal executables

  Refers to executables that should not be directly invoked by users in the terminal. These executables **SHALL NOT** be accessible via `$PATH`.

  ```makefile
  install:
      $(INSTALL) -d "$(DESTDIR)$(libexecdir)"
      $(INSTALL_PROGRAM) path/to/my/internal/executable "$(DESTDIR)$(libexecdir)"/executable
  ```

- Static libraries

  ```makefile
  install:
      $(INSTALL) -d "$(DESTDIR)$(libdir)"
      $(INSTALL_DATA) path/to/my/library.a "$(DESTDIR)$(libdir)"/library.a
  ```

- pkg-config configuration files

  ```makefile
  pc_dir ?= $(libdir)/pkgconfig
  ifeq ($(findstring $(pc_dir), $(subst :, , $(shell \
      $(PKG_CONFIG) --variable=pc_path)), )
  $(warning pc_dir="$(pc_dir)" \
      is not in the search path of current pkg-config installation)
  endif

  .PHONY: install-pkg-config
  install-pkg-config:
      $(INSTALL) -d "$(DESTDIR)$(pc_dir)"
      $(INSTALL_DATA) path/to/your.pc "$(DESTDIR)$(pc_dir)"/your.pc
  ```

  If it can be determined that the file is cross-architecture compatible, `$(datarootdir)` **MAY** be used instead of `$(libdir)`.

- System-level systemd units

  ```makefile
  systemd_system_unit_dir ?= $(shell \
      $(PKG_CONFIG) --define-variable=prefix=$(prefix) \
      systemd --variable=systemd_system_unit_dir)
  ifeq ($(findstring $(systemd_system_unit_dir), $(subst :, , $(shell \
      $(PKG_CONFIG) systemd --variable=systemd_system_unit_path))), )
  $(warning systemd_system_unit_dir="$(systemd_system_unit_dir)" \
      is not in the system unit search path of current systemd installation)
  endif

  .PHONY: install-systemd-system-unit
  install-systemd-system-unit:
      $(INSTALL) -d "$(DESTDIR)$(systemd_system_unit_dir)"
      $(INSTALL_DATA) path/to/your.service "$(DESTDIR)$(systemd_system_unit_dir)"/your.service
  ```

- User-level systemd units

  ```makefile
  systemd_user_unit_dir ?= $(shell \
      $(PKG_CONFIG) --define-variable=prefix=$(prefix) \
      systemd --variable=systemd_user_unit_dir)
  ifeq ($(findstring $(systemd_user_unit_dir), $(subst :, , $(shell \
      $(PKG_CONFIG) systemd --variable=systemd_user_unit_path))), )
  $(warning systemd_user_unit_dir="$(systemd_user_unit_dir)" \
      is not in the user unit search path of current systemd installation)
  endif

  .PHONY: install-systemd-user-unit
  install-systemd-user-unit:
      $(INSTALL) -d "$(DESTDIR)$(systemd_user_unit_dir)"
      $(INSTALL_DATA) path/to/your.service "$(DESTDIR)$(systemd_user_unit_dir)"/your.service
  ```

- Desktop files

  ```makefile
  install:
      $(INSTALL) -d "$(DESTDIR)$(datarootdir)"/applications
      $(INSTALL_PROGRAM) path/to/your.desktop "$(DESTDIR)$(datarootdir)"/applications/your.desktop
  ```

- Desktop file icons

  See: <https://specifications.freedesktop.org/icon-theme-spec/latest/#install_icons>
  - If installing icons of fixed sizes, using PNG format **IS RECOMMENDED**

    At least a 48x48 PNG **MUST** be installed to ensure proper basic functionality of icon-related features in the desktop environment

  - If installing vector versions of icons, using SVG format **IS RECOMMENDED**

  ```makefile
  install:
      $(INSTALL) -d "$(DESTDIR)$(datarootdir)"/icons/hicolor/48x48/apps
      $(INSTALL_DATA) path/to/your.png "$(DESTDIR)$(datarootdir)"/icons/hicolor/48x48/apps/your.png
      # Add more sizes of .png icons here...

      $(INSTALL) -d "$(DESTDIR)$(datarootdir)"/icons/hicolor/scalable/apps
      $(INSTALL_DATA) path/to/your.svg "$(DESTDIR)$(datarootdir)"/icons/hicolor/scalable/apps/your.svg
  ```

#### CMake

This section is mainly referenced from content in <https://cmake.org/cmake/help/v3.30/module/GNUInstallDirs.html#module:GNUInstallDirs> and <https://wiki.debian.org/Multiarch/Implementation>.

Here we define default values for some variables and other common parts for writing examples later. Explanations of these variable defaults can be found in the links above.

The writing logic is consistent with the related content in the Makefile section.

```cmake
include(GNUInstallDirs)
```

- Executable files

  See: <https://cmake.org/cmake/help/v3.30/command/install.html#programs>

  ```cmake
  install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/path/to/your/executable TYPE BIN)
  ```

- Internal executables

  Refers to executables that should not be directly invoked in the terminal. These executables **SHALL NOT** be accessible via `$PATH`

  See: <https://cmake.org/cmake/help/v3.30/command/install.html#programs>

  ```cmake
  install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/path/to/your/executable DESTINATION ${CMAKE_INSTALL_LIBEXECDIR})
  ```

  Note: TYPE is not used because TYPE does not currently support LIBEXEC

- Target files/Export files

  To be supplemented

- pkg-config configuration files

  To be supplemented

- System-level systemd units

  ```cmake
  find_package(PkgConfig)
  if(NOT SYSTEMD_SYSTEM_UNIT_DIR)
      if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.28")
          pkg_get_variable(SYSTEMD_SYSTEM_UNIT_DIR systemd systemd_system_unit_dir
                           DEFINE_VARIABLES prefix=${CMAKE_INSTALL_PREFIX})
      else()
          pkg_get_variable(SYSTEMD_PREFIX systemd prefix)
          pkg_get_variable(SYSTEMD_SYSTEM_UNIT_DIR systemd systemd_system_unit_dir)
          string(REPLACE "${SYSTEMD_PREFIX}" "${CMAKE_INSTALL_PREFIX}"
                 SYSTEMD_SYSTEM_UNIT_DIR "${SYSTEMD_SYSTEM_UNIT_DIR}")
      endif()
  endif()
  pkg_get_variable(SYSTEMD_SYSTEM_UNIT_PATH systemd systemd_system_unit_path)
  if(NOT SYSTEMD_SYSTEM_UNIT_PATH MATCHES ".*:${SYSTEMD_SYSTEM_UNIT_DIR}:.*")
      message(WARNING SYSTEMD_SYSTEM_UNIT_DIR="${SYSTEMD_SYSTEM_UNIT_DIR}" is not
                      in the system unit search path of
                      current systemd installation)
  endif()

  install(FILES path/to/your.service
          DESTINATION ${SYSTEMD_SYSTEM_UNIT_DIR})
  ```

- User-level systemd units

  ```cmake
  find_package(PkgConfig)
  if(NOT SYSTEMD_USER_UNIT_DIR)
      if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.28")
          pkg_get_variable(SYSTEMD_USER_UNIT_DIR systemd systemd_user_unit_dir
                           DEFINE_VARIABLES prefix=${CMAKE_INSTALL_PREFIX})
      else()
          pkg_get_variable(SYSTEMD_PREFIX systemd prefix)
          pkg_get_variable(SYSTEMD_USER_UNIT_DIR systemd systemd_user_unit_dir)
          string(REPLACE "${SYSTEMD_PREFIX}" "${CMAKE_INSTALL_PREFIX}"
                 SYSTEMD_USER_UNIT_DIR "${SYSTEMD_USER_UNIT_DIR}")
      endif()
  endif()
  pkg_get_variable(SYSTEMD_USER_UNIT_PATH systemd systemd_user_unit_path)
  if(NOT SYSTEMD_USER_UNIT_PATH MATCHES ".*:${SYSTEMD_USER_UNIT_DIR}:.*")
      message(WARNING SYSTEMD_USER_UNIT_DIR="${SYSTEMD_USER_UNIT_DIR}" is not
                      in the user unit search path of
                      current systemd installation)
  endif()

  install(FILES path/to/your.service
          DESTINATION ${SYSTEMD_USER_UNIT_DIR})
  ```

- Desktop files

  ```cmake
  install(FILES path/to/your.desktop
          DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
  ```

- Desktop file icons

  ```cmake
  install(FILES path/to/your.png
          DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/48x48/apps)
  # Add more sizes of .png icons here...

  install(FILES path/to/your.svg
          DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
  ```

### Configuration Files

#### Desktop files

Desktop file names **SHALL NOT** contain `-`. After removing the `.desktop` suffix, they **SHALL** comply with the relevant specifications described in the Application Name section.

- **RECOMMENDED** to fill the [`TryExec` field][key-tryexec] to ensure that the desktop file is no longer valid after the application has been uninstalled
- **RECOMMENDED** to fill the [`WMClass` field][key-startupwmclass] to ensure that desktop environment basic functions based on window and application matching, such as taskbar, work properly
- **RECOMMENDED** to use only the executable file name rather than absolute paths when filling the [`Exec` field][key-exec]
- **NOT RECOMMENDED** to use absolute paths when filling the [`Icon` field][key-icon]

[key-tryexec]: https://specifications.freedesktop.org/desktop-entry-spec/latest/recognized-keys.html#key-tryexec
[key-startupwmclass]: https://specifications.freedesktop.org/desktop-entry-spec/latest/recognized-keys.html#key-startupwmclass
[key-exec]: https://specifications.freedesktop.org/desktop-entry-spec/latest/recognized-keys.html#key-exec
[key-icon]: https://specifications.freedesktop.org/desktop-entry-spec/latest/recognized-keys.html#key-icon

#### DBus Services

- **RECOMMENDED** that service file names match the Name field in the file
- **RECOMMENDED** to use absolute paths in the Exec field of service files
- **RECOMMENDED** to fill the SystemdService field
- **RECOMMENDED** that the service name in the SystemdService field matches the Name field
- **RECOMMENDED** that the Name field ends with a number

#### Systemd Services

- **RECOMMENDED** that service files with BusName have file names matching the BusName
- **RECOMMENDED** to use absolute paths in the ExecStart field

When the above configuration files use absolute paths, **NOT RECOMMENDED** to hardcode paths. Their paths **SHALL** remain consistent with the final installation location. **RECOMMENDED** to first write template files in the project, using placeholders to represent absolute paths, and generate final configuration files after the build system replaces the placeholders.

Here we use desktop files as examples to show how to generate final configuration files under several common build systems.

Assume the final product `org.deepin.demo.desktop` content is as follows:

```ini
[Desktop Entry]
Name=demo
Exec=/usr/bin/demo
Type=Application
Terminal=false
```

- Using Makefile as the build system.
  1. First, write the template file `org.deepin.demo.desktop.in` with the following content:

  ```ini
  [Desktop Entry]
  Name=demo
  Exec=@BINDIR@/demo
  Type=Application
  Terminal=false
  ```

  2. Write the corresponding makefile rules.

  ```makefile
  DESKTOP_TEMPLATE = org.deepin.demo.desktop.in
  DESKTOP_FILE = org.deepin.demo.desktop

  # Replace placeholders and generate the final .desktop file
  desktop: $(DESKTOP_TEMPLATE)
    sed -e "s/@BINDIR@/$(bindir)/g" \
        $(DESKTOP_TEMPLATE) > $(DESKTOP_FILE)

  install:
      $(INSTALL) -d "$(DESTDIR)$(datarootdir)"/applications
      $(INSTALL_PROGRAM) $(DESKTOP_FILE) "$(DESTDIR)$(datarootdir)"/applications/$(DESKTOP_FILE)

  clean:
    rm -f $(DESKTOP_FILE)

  all: desktop
  ```

- If using CMake as the build system.
  1. Write the desktop template file.

  ```desktop
  [Desktop Entry]
  Name=demo
  Exec=@CMAKE_INSTALL_BINDIR@/demo
  Type=Application
  Terminal=false
  ```

  2. Write the corresponding cmake rules.

  ```cmake
  set(DESKTOP_FILE "org.deepin.demo.desktop")
  # Use configure_file for placeholder replacement
  configure_file(
      ${CMAKE_CURRENT_SOURCE_DIR}/org.deepin.demo.desktop.in
      ${CMAKE_CURRENT_BINARY_DIR}/${DESKTOP_FILE}
      @ONLY
  )
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${DESKTOP_FILE}
          DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
  ```

### Header Files and Libraries

The Linyaps environment consists of up to three parts. Taking compiling `org.deepin.demo` under the `x86_64` architecture as an example, the default search paths for header files and library files include the following parts:

| **Component**      | **Package Name**       | **Header Files**                        | **Library Files**                                                                           |
| ------------------ | ---------------------- | --------------------------------------- | ------------------------------------------------------------------------------------------- |
| base               | org.deepin.base        | /usr/include                            | /usr/lib<br>/usr/lib/x86_64-linux-gnu                                                       |
| runtime (optional) | org.deepin.runtime.dtk | /runtime/include                        | /runtime/lib<br>/runtime/lib/x86_64-linux-gnu                                               |
| app                | org.deepin.demo        | /opt/apps/org.deepin.demo/files/include | /opt/apps/org.deepin.demo/files/lib<br>/opt/apps/org.deepin.demo/files/lib/x86_64-linux-gnu |

Priority is arranged from top to bottom. If a header file exists in both `org.deepin.base` and `org.deepin.demo`, the file in `org.deepin.demo` will be matched with higher priority during use. The same applies to library files.

Default search paths are suitable for standard libraries or development libraries without configuration files. In actual build scenarios, development libraries usually provide configuration files to facilitate users' compilation and linking. Developers **SHALL** use these files in their build systems rather than relying on default search paths.

Common configuration files include `.pc`, `.cmake`, etc. How to use them specifically depends on the development library and build system. Here we give examples of using configuration files with several common build systems.

#### Makefile

##### Using `.pc` files

```makefile
# Common variables, inherit CXXFLAGS environment variable and append content
CXX = g++
CXXFLAGS = $(CXXFLAGS) -Wall -Wextra -std=c++11
# Get .pc file content through pkg-config tool
# Return value is generally content of -I/path -lname type
PKG_CONFIG = pkg-config
LIBS = $(shell $(PKG_CONFIG) --cflags --libs libname)

TARGET = demo

SRCS = main.cpp

all: $(TARGET)

# Provide .pc information to compiler during build
$(TARGET): $(SRCS)
 $(CXX) $(CXXFLAGS) $(LIBS) -o $@ $^

clean:
 rm -f $(TARGET)
```

#### CMake

##### Using `.pc` files

```cmake
find_package(PkgConfig REQUIRED)
pkg_check_modules(PackageAliasName REQUIRED IMPORTED_TARGET PackageName)

# Add executable
add_executable(demo main.cpp)

# Set linking libraries. Header file search paths will be automatically expanded.
target_link_libraries(demo PRIVATE PkgConfig::PackageAliasName)
```

##### Using `.cmake` files

```cmake
find_package(<PackageName> REQUIRED COMPONENTS <Component>)

# Add executable
add_executable(demo main.cpp)

# Set linking libraries. Header file search paths will be automatically expanded.
target_link_libraries(demo PRIVATE PackageName::Component)
```

Note:

**NOT RECOMMENDED** to directly hardcode the above default search paths in project build configuration files.
**NOT RECOMMENDED** to override environment variables such as CFLAGS/CXXFLAGS. If there are additional parameters, they **SHALL** be appended to these variables.

### Dependency Introduction

As mentioned in the "Header Files and Libraries" section, the Linyaps environment consists of multiple parts. If the content in base or even runtime cannot meet requirements, developers **SHALL** introduce missing dependencies on the APP side. The introduction method is to declare them under the `sources` field in the `linglong.yaml` file and write corresponding installation or compilation rules under the `build` field.

`sources` supports multiple file types, **MAY** introduce source code or compiled binary files, even a deb. However, for introducing compiled binary files, developers **MUST** consider whether they are compatible with the current base or runtime.

#### Using Source Code Introduction

Introducing dependencies through source code is a **RECOMMENDED** practice, as it can greatly ensure the stability and maintainability of the build process. The downside is that it may take developers considerable time to write yaml files, as dependencies may have their own dependencies.

_If developers find that dependencies are complex and repeatedly used by other applications, they **SHOULD** consider integrating the dependencies into a runtime-type package._

When dependencies are compiled under the Linyaps environment, their configuration files are usually "reliable". After compilation and installation, developers can directly use them in their projects.

```yaml
sources:
  - kind: git
    url: # app source url
    commit: # commit hash
  - kind: git
    url: # dependency source url
    commit: # commit hash
build:
  # Compile dependency
  cd /project/your_dependency_name/
  cmake -Bbuild -DCMAKE_INSTALL_PREFIX=$PREFIX
  make install

  # Compile and install APP
  cd /project/your_app_name
  cmake -Bbuild -DCMAKE_INSTALL_PREFIX=$PREFIX
  make install
```

#### Using deb Introduction

This is a "shortcut". If developers do not consider subsequent updates of the application, this method **MAY** be used. Developers can use auxiliary tools to analyze dependency relationships and batch import the involved dependencies into sources.

##### Using VSCode Linyaps Extension

1. Install the aptly command-line tool
2. Search for linglong in the VSCode extension store and install related plugins.
3. Place the sources field at the end of linglong.yaml
4. Add gen_deb_source comment at the end of sources
5. Press Ctrl+Shift+P and search for linglong: Gen deb sources command

After execution, the yaml file will automatically write the following content:

```yaml
build:
  # Extract and import deb, install_dep file is automatically downloaded by the plugin
  bash ./install_dep linglong/sources "$PREFIX"

  # Compile and install APP
sources:
  # Source code
  - kind: git
    url: # app source url
    commit: # commit hash
  # The following comments are generated and used by the plugin, retrieving libical-dev from repository to analyze its dependencies, automatically generating sources below the comments
  # linglong:gen_deb_source sources amd64 https://ci.deepin.com/repo/deepin/deepin-community/backup/rc2 beige main
  # linglong:gen_deb_source install libical-dev
```

Note:
The compiled products of deb have an installation prefix of `/usr`. The `install_dep` script automatically handles `.pc` files in it, replacing `/usr` with `$PREFIX`. `xxx.cmake` type files cannot be batch processed. If there are hardcoded path behaviors, they may not work properly.

## Linyaps

This section mainly describes some special requirements of the Linyaps package management system for applications.

### Limitations

- Applications that need to be packaged through the Linyaps package management system **MUST** run as the user who started the Linyaps container (usually a regular user). Any form of privilege escalation mechanism is unavailable when the Linyaps package management system runs applications, including but not limited to SUID bits recorded in the file system and capabilities and other permission escalation mechanisms based on file system extended attributes.
- Linyaps currently does not support packaging applications containing system-level systemd units.
- **NOT RECOMMENDED** to package Desktop Environment components, such as launchers, file managers, resource managers, status bars, etc.

### Specifications

#### Application Package Name

Linyaps application package names **MUST** fully comply with the relevant regulations in the "Application Name" section above. Additionally:

- Application names are case-insensitive

#### Application Version Number

Linyaps package application version numbers **MUST** be four groups of decimal numbers separated by `.`, for example `1.0.0.0`.

If the specified version number has fewer than four groups, zeros will be supplemented backward until the conditions are met, for example `1.90` will be automatically supplemented to `1.90.0.0`.

The overall length of the version number after automatic supplementation to four groups **SHALL NOT** exceed 256 bytes in string meaning.

#### Runtime Environment

Linyaps applications **MUST** select a base as the basic runtime environment. Available bases:

| **Base Library** | **Package Name/Version** |
| ---------------- | ------------------------ |
| glibc(2.38)      | org.deepin.base/23.1.0.0 |

If frameworks beyond the basic environment need to be used additionally, **SHALL** use appropriate runtime. Available runtime:

| **Framework**       | **Package Name/Version**        |
| ------------------- | ------------------------------- |
| QT(5.15) + DTK(5.6) | org.deepin.runtime.dtk/23.1.0.0 |

When using base or runtime, version numbers **SHALL** fill the first three digits, such as '23.1.0', to facilitate receiving updates later. Filling all four digits version means **FORBIDDEN** base or runtime updates.

#### Installation Location

During the build and installation process, Linyaps build tools will set the `$PREFIX` environment variable. The value of this environment variable **SHALL** be `/opt/apps/${APPID}/files`, but it is not recommended to directly use its common value when writing build and installation processes. **RECOMMENDED** to always read the `$PREFIX` environment variable.

For the application's build and installation process, writable directories are only:

1. The `/source` directory for placing source code required for building
2. The installation location, i.e., the location specified by `$PREFIX`

Installing files to other locations in the build process is **FORBIDDEN**, which usually leads to write failures or applications being unable to find these files at runtime.

The following are examples of using the `$PREFIX` environment variable to control application installation location when calling some common build systems:

##### cmake

```bash
cd path/to/build && cmake path/to/source -DCMAKE_INSTALL_PREFIX="$PREFIX"
```

##### make

```bash
cd path/to/source && make prefix="$PREFIX"
```

##### meson

```bash
cd path/to/source && meson configure --prefix="$PREFIX" path/to/build
```

## Build Artifacts

This section mainly describes the composition and related functions of application build artifacts after successful build.

### Directory Structure

After successful application build, Linyaps build tools will submit the artifacts to local cache. When distributing offline, they should be exported as `.layer` or `.uab` type files.

By extracting the build artifact layer file:

```bash
ll-builder extract org.deepin.demo_0.0.0.1_x86_64_binary.layer ./tmp
```

The following directory structure can be obtained:

```plain
  ./tmp
  ├── entries
  │   └── share -> ../files/share
  ├── files
  │   ├── bin
  │   │   └── demo
  │   ├── share
  │   │   ├── applications
  │   │   │   └── org.deepin.demo.desktop
  │   │   ├── icons
  │   │   │   └── hicolor
  │   │   │       └── scalable
  │   │   │           └── apps
  │   │   │               └── org.deepin.demo.svg
  │   │   ├── doc
  │   │   │   ├── changelog.gz
  │   │   │   └── copyright
  │   │   ├── mime
  │   │   │   └── packages
  │   │   │        └── org.deepin.demo.xml
  │   │   ├── locale
  │   │   │   └── zh_CN
  │   │   │       └── info.json
  │   │   └── services
  │   │       └── org.deepin.demo.xml
  │   └── libs
  │       └── libdemo.so.5.2.1
  ├── info.json
  └── org.deepin.demo.install
```

When the application runs, these files or directories will be mapped to the following paths in the container:

```plain
/
├── bin
├── ...
├── opt
│   └── apps
│       └── org.deepin.demo
│            ├── files
│            ├── entries
│            ├── info.json
│            └── org.deepin.demo.install
└── var
```

#### info.json file

info.json is an application description file defined by Linyaps. This file is automatically generated by build tools and **SHALL NOT** be manually modified. Its content is as follows:

```json
{
  "id": "org.deepin.demo",
  "arch": ["x86_64"],
  "base": "main:org.deepin.foundation/23.0.0/x86_64",
  "channel": "main",
  "command": ["/opt/apps/org.deepin.demo/files/bin/demo"],
  "description": "simple Qt demo.\n",
  "kind": "app",
  "module": "runtime",
  "name": "demo",
  "runtime": "main:org.deepin.Runtime/23.0.1/x86_64",
  "size": 118763,
  "version": "0.0.0.1"
}
```

Below is a sequential explanation of each field in info.json:

`id`: Software package identifier, i.e., application package name.

`arch`: Supported architectures for the software package. Currently supports the following CPU architectures:

- `amd64`: Applicable to `x86_64` architecture `CPU`.
- `loongarch64`: Applicable to new Loongson series `CPU`.
- `arm64`: Applicable to `ARM64` bit `CPU`.

`base`: Basic environment used by the software package at runtime.

`channel`: Software package distribution channel.

`command`: Default startup command for the software package.

`description`: Description information of the software package.

`kind`: Software package category.

`module`: Software package module.

`name`: Common name of the software package.

`runtime`: Environment used by the software package at runtime.

`size`: Software package size.

`version`: Application version, whose format shall meet the requirements described in the Application Version Number section.

#### entries directory

This directory is used to share application configuration files with the host (desktop environment). This directory usually has the following subdirectories:

- entries/share/applications
- entries/share/dbus-1/services
- entries/share/systemd/user
- entries/share/icons
- entries/share/mime
- entries/share/fonts

The entries directory is automatically generated by build tools. The directory only contains a soft link named share, which points to the files/share directory in the upper level directory.

When an application is installed on the host, the Linyaps package manager will use this link file to link all files in it to the path added by Linyaps to the `$XDG_DATA_DIRS` variable. That is, `/var/lib/linglong/entries/share/`.

```bash
$ ls /var/lib/linglong/entries/share/applications/ -l
lrwxrwxrwx 1 deepin-linglong deepin-linglong 101 July 30 11:13 org.deepin.demo.desktop -> ../../../layers/main/org.deepin.demo/0.0.0.1/x86_64/runtime/entries/share/applications/org.deepin.demo.desktop
```

##### applications directory

Place application startup configuration files, i.e., .desktop files.

```ini
[Desktop Entry]
Exec=demo
Name=demo
TryExec=demo
Type=Application
```

This file will be automatically modified during build to:

```ini
[Desktop Entry]
Exec=/usr/bin/ll-cli run org.deepin.demo -- demo
Name=demo
TryExec=/usr/bin/ll-cli
Type=Application
```

Applications can have multiple desktop files.

**Path correspondence:**

| **Packaging Path**                                 | **Installation Path**                                     |
| -------------------------------------------------- | --------------------------------------------------------- |
| $PREFIX/share/applications/org.deepin.demo.desktop | $XDG_DATA_DIRS/share/applications/org.deepin.demo.desktop |

##### dbus services directory

Directory for dbus services registered by programs, for example:

```ini
[D-BUS Service]
Name=org.deepin.demo
Exec=/opt/apps/org.deepin.demo/files/bin/demo --dbus
```

This file will be automatically modified during build to:

```ini
[D-BUS Service]
Name=org.deepin.demo
Exec=/usr/bin/ll-cli run org.deepin.demo -- /opt/apps/org.deepin.demo/files/bin/demo --dbus
```

An application can configure multiple services. Service names must be subdomains.

**Path correspondence:**

| **Packaging Path**                                   | **Installation Path**                                       |
| ---------------------------------------------------- | ----------------------------------------------------------- |
| $PREFIX/share/services/org.deepin.demo.service       | $XDG_DATA_DIRS/dbus-1/service/org.deepin.demo.service       |
| $PREFIX/share/services/org.deepin.demo.hello.service | $XDG_DATA_DIRS/dbus-1/service/org.deepin.demo.hello.service |

##### User-level systemd services

Directory for user-level services registered by programs, for example:

```ini
[Unit]
Description = demo service
After=user-session.target

[Service]
Type = simple
ExecStart = demo

[Install]
WantedBy=user-session.target
```

This file will be automatically modified during build to:

```ini
[Unit]
Description = demo service
After=user-session.target

[Service]
Type = simple
ExecStart = ll-cli run org.deepin.demo -- demo

[Install]
WantedBy=user-session.target
```

Unlike dbus service, files installed to `$PREFIX/lib/systemd/user` will be automatically copied to `$PREFIX/share/systemd/user`.

**Path correspondence:**

| **Packaging Path**                               | **Installation Path**                               |
| ------------------------------------------------ | --------------------------------------------------- |
| $PREFIX/lib/systemd/user/org.deepin.demo.service | $XDG_DATA_DIRS/systemd/user/org.deepin.demo.service |

##### icons directory

Directory for application icons. The structure should be consistent with the system icons directory structure.

**Path correspondence:**

| **Packaging Path**                                            | **Installation Path**                                          |
| ------------------------------------------------------------- | -------------------------------------------------------------- |
| $PREFIX/share/icons/hicolor/scalable/apps/org.deepin.demo.svg | $XDG_DATA_DIRS/icons/hicolor/scalable/apps/org.deepin.demo.svg |
| $PREFIX/share/icons/hicolor/24x24/apps/org.deepin.demo.png    | $XDG_DATA_DIRS/icons/hicolor/24x24/apps/org.deepin.demo.png    |
| $PREFIX/share/icons/hicolor/16x16/apps/org.deepin.demo.png    | $XDG_DATA_DIRS/icons/hicolor/16x16/apps/org.deepin.demo.png    |

##### mime directory

MIME (Multipurpose Internet Mail Extensions) multipurpose internet mail extensions type. This directory is used to store mime configuration files, which are in XML format and end with .xml.

**Path correspondence:**

| **Packaging Path**                              | **Installation Path**                            |
| ----------------------------------------------- | ------------------------------------------------ |
| $PREFIX/share/mime/packages/org.deepin.demo.xml | $XDG_DATA_DIRS/mime/packages/org.deepin.demo.xml |

##### fonts directory

Font storage path.

#### files directory

Store various files required by the application. There are no restrictions on placing files in this directory, but it is recommended to place executable programs in the bin subdirectory. Third-party libraries that applications or plugins depend on are recommended to be placed in /opt/apps/${id}/files/lib directory.

#### .install file

The org.deepin.demo.install file in the above example org.deepin.demo is a file automatically generated during the build process. This file is used to define which files should be installed in the binary module and can be used for final software package artifact volume trimming.

When this file is not defined in the same directory level as linglong.yaml, all content is installed to the binary module according to the rules under build in linglong.yaml.

Usage method:

After the first successful build, an .install file will be generated in the artifacts and record all files installed to the binary module. Copy this file to the directory level of linglong.yaml. After modifying the content in the .install file and building again, only the content noted in .install will be submitted to the binary module.

## References

[Desktop Application Packaging Specifications](https://github.com/linuxdeepin/deepin-specifications/blob/master/unstable/%E6%A1%8C%E9%9D%A2%E5%BA%94%E7%94%A8%E6%89%93%E5%8C%85%E8%A7%84%E8%8C%83.md)
