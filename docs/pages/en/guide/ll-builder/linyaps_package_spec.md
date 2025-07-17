# Linglong Application Packaging Specification

The explanations of the keywords **must**, **prohibited**, **necessary**, **should**, **should not**, **recommended**, **allowed**, and **optional**[^rfc2119-keywords] in this document can be found in the description in [RFC 2119][rfc-2119].

The correspondence between these keywords and the English words in the original text is shown in the following table:

| Chinese         | English     |
| --------------- | ----------- |
| **Must**        | MUST        |
| **Prohibited**  | MUST NOT    |
| **Necessary**   | REQUIRED    |
| **Should**      | SHALL       |
| **Should not**  | SHALL NOT   |
| **Recommended** | RECOMMENDED |
| **Allowed**     | MAY         |
| **Optional**    | OPTIONAL    |

[rfc-2119]: https://datatracker.ietf.org/doc/html/rfc2119

This document is intended to help application developers standardize the behavior of the application build process in order to migrate to the Linglong package management system.

## General

This section mainly records some general specifications that almost all projects should follow.

### Application name

The application name **should** use [reverse domain name notation](https://zh.wikipedia.org/wiki/%E5%8F%8D%E5%90%91%E5%9F%9F%E5%90%8D%E8%A1%A8%E7%A4%BA%E6%B3%95), such as `org.deepin.demo`.

This application name is mainly used in scenarios such as naming desktop files and DBus services.

- Developers **should** use the reverse of the domain name they actually own as the prefix of the application name, followed by the application name

**Note**: If the developer cannot prove that they actually own the domain name, the application package may be removed from the repository.

- For third-party applications developed on GitHub, if the organization where the application is located has an additional domain name, it should be used first, otherwise `io.github.<GITHUB_ID>.` should be used as the prefix.

In particular, if the organization name and application name of the application are the same, such as <https://github.com/neovim/neovim>, the packager should not omit the duplicate application name and organization name, and the ID of this application should be `io.github.neovim.neovim`.

**Note**: In fact, the organization owns the domain name `neovim.io`, so the most reasonable application name **should** be `io.neovim.neovim`.

- **Not recommended** to use application names containing `-`, if the domain name/application name does contain `-`, **recommend** to use `_` instead

- **Not recommended** application names ending with `.desktop`

The above specifications are from [Desktop Entry Specification][desktop-entry-specification].

[desktop-entry-specification]: https://specifications.freedesktop.org/desktop-entry-spec/latest

Further reading: <https://docs.flatpak.org/en/latest/conventions.html#application-ids>

### `prefix` and `$DESTDIR`

When writing the build process of an application, developers **should** not assume that the installation location is fixed. It is not standard to install executable files to a hard-coded path such as `/usr/bin` in Makefile/CMakeLists.txt.

When writing the build/installation process, developers **should** respect the build system's `prefix` configuration and the value of the `$DESTDIR` environment variable so that packagers can easily configure the specific installation location of the application when packaging.

`prefix` refers to the specific location specified to the build system during build/installation where the application will eventually be installed to the system.

When the developer does not specify the installation location, the default value should be `/usr/local`.
When packaging through the package management system, the package management system will configure its value. When using dpkg-related tools for packaging, its value will be configured as `/usr`. However, when writing the build/installation process, the developer should consider the case where `prefix` is configured to any value.

`$DESTDIR` refers to an environment variable agreed upon when the build system is installed to facilitate the distribution packaging process. Its general working logic is as follows:

If the build system completes the build work and executes the installation process, it is specified with `prefix=/usr` and `$DESTDIR=./tmp`, then after the installation is completed, all product files should appear in the `./tmp/usr` directory. The packaging tool will then treat `./tmp` as the root directory and compress and package the files in it.

Here, taking the common build system as an example, several **recommended** ways of writing common file installation processes are given. These writing methods all respect the `prefix` configuration and `$DESTDIR`:

#### Makefile

This section is mainly written with reference to the relevant content in <https://www.gnu.org/prep/standards/standards.html> and <https://wiki.debian.org/Multiarch/Implementation>.

Here, the default values ​​of some variables and other common parts are defined for the purpose of writing examples later. The relevant descriptions of the default values ​​of these variables can be found in the above link.

```makefile
DESTDIR ?=

prefix ?= /usr/local
bindir ?= $(prefix)/bin
libdir ?= $(prefix)/lib
libexecdir ?= $(prefix)/libexec
datarootdir ?= $(prefix)/share

INSTALL ?= install
INSTALL_PROGRAM ?= $(INSTALL)
INSTALL_DATA ?= $(INSTALL) -m 644

# The variables with default values ​​defined above are the recommended behaviors in the above specifications, and no modification is recommended.

PKG_CONFIG ?= pkg-config

.PHONY: install
```

- Executable files

```makefile
install:
$(INSTALL) -d "$(DESTDIR)$(bindir)"
$(INSTALL_PROGRAM) path/to/your/executable "$(DESTDIR)$(bindir)"/executable
```

- Internal executable files

Refers to executable files that should not be called directly by users in the terminal. These executable files **should** not be found through `$PATH`

```makefile
install:
$(INSTALL) -d "$(DESTDIR)$(libexecdir)"
$(INSTALL_PROGRAM) path/to/my/internal/executable "$(DESTDIR)$(libexecdir)"/executable
```

- Static libraries

````makefile
install:
$(INSTALL) -d "$(DESTDIR)$(libdir)" $(INSTALL_DATA) path/to/my/library.a "$(DESTDIR)$(libdir)"/library.a ``` - pkg-config configuration file ```makefile pc_dir ?= $(libdir)/pkgconfig ifeq ($(findstring $(pc_dir), $(subst :, , $(shell \ $(PKG_CONFIG) --variable=pc_path)), ) $(warning pc_dir="$(pc_dir)" \ is not in the search path of current pkg-config installation) endif .PHONY: install-pkg-config install-pkg-config: $(INSTALL) -d "$(DESTDIR)$(pc_dir)" $(INSTALL_DATA) path/to/your.pc "$(DESTDIR)$(pc_dir)"/your.pc
````

If you are sure that the file is available across architectures, you can use `$(datarootdir)` instead of `$(libdir)`.

- systemd system level unit `makefile systemd_system_unit_dir ?= $(shell \ $(PKG_CONFIG) --define-variable=prefix=$(prefix) \ systemd --variable=systemd_system_unit_dir) ifeq ($(findstring $(systemd_system_unit_dir), $(subst :, , $(shell \ $(PKG_CONFIG) systemd --variable=systemd_system_unit_path))), ) $(warning systemd_system_unit_dir="$(systemd_system_unit_dir)" \ is not in the system unit search path of current systemd installation) endif .PHONY: install-systemd-system-unit install-systemd-system-unit: $(INSTALL) -d "$(DESTDIR)$(systemd_system_unit_dir)" $(INSTALL_DATA) path/to/your.service "$(DESTDIR)$(systemd_system_unit_dir)"/your.service ` - systemd user-level unit ```makefile systemd_user_unit_dir ?= $(shell \ $(PKG_CONFIG) --define-variable=prefix=$(prefix) \ systemd --variable=systemd_user_unit_dir) ifeq ($(findstring $(systemd_user_unit_dir), $(subst :, , $(shell \ $(PKG_CONFIG) systemd --variable=systemd_user_unit_path))), ) $(warning systemd_user_unit_dir="$(systemd_user_unit_dir)" \ is not in the user unit search path of current systemd installation) endif .PHONY: install-systemd-user-unit install-systemd-user-unit: $(INSTALL) -d "$(DESTDIR)$(systemd_user_unit_dir)" $(INSTALL_DATA) path/to/your.service "$(DESTDIR)$(systemd_user_unit_dir)"/your.service

````

- desktop file

```makefile
install:
$(INSTALL) -d "$(DESTDIR)$(datarootdir)"/applications
$(INSTALL_PROGRAM) path/to/your.desktop "$(DESTDIR)$(datarootdir)"/applications/your.desktop
````

- desktop file corresponding icon

See: <https://specifications.freedesktop.org/icon-theme-spec/latest/#install_icons>

- If the installed icon is a fixed size version, it is **recommended** to use png format

At least **need** to install a 48x48 size png to ensure the normal basic functions of icons in the desktop environment

- If the installed icon is a vector version, it is **recommended** to use svg format

````makefile
install: $(INSTALL) -d "$(DESTDIR)$(datarootdir)"/icons/hicolor/48x48/apps $(INSTALL_DATA) path/to/your.png "$(DESTDIR)$(datarootdir)"/icons/hicolor/48x48/apps/your.png # Add more size of .png icons here... $(INSTALL) -d "$(DESTDIR)$(datarootdir)"/icons/hicolor/scalable/apps $(INSTALL_DATA) path/to/your.svg "$(DESTDIR)$(datarootdir)"/icons/hicolor/scalable/apps/your.svg ``` #### CMake

This section is mainly written with reference to the relevant content in <https://cmake.org/cmake/help/v3.30/module/GNUInstallDirs.html#module:GNUInstallDirs> and <https://wiki.debian.org/Multiarch/Implementation>.

Here we define the default values ​​of some variables and other common parts for writing examples later. The relevant descriptions of these variable default values ​​can be found in the link above.

The writing logic is consistent with the relevant content in the Makefile section.

```cmake
include(GNUInstallDirs)
````

- Executable files

See: <https://cmake.org/cmake/help/v3.30/command/install.html#programs>

```cmake
install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/path/to/your/executable TYPE BIN)
```

- Internal executable files

Refers to executable files that should not be called directly in the terminal. These executable files **should** not be found through `$PATH`

See: <https://cmake.org/cmake/help/v3.30/command/install.html#programs>

```cmake
install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/path/to/your/executable DESTINATION ${CMAKE_INSTALL_LIBEXECDIR})
```

Note: TYPE is not used because TYPE does not currently support LIBEXEC

- Target file/export file

To be added

- pkg-config configuration file

To be added

- systemd system-level unit

````cmake
find_package(PkgConfig)
if(NOT SYSTEMD_SYSTEM_UNIT_DIR)
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.28")
pkg_get_variable(SYSTEMD_SYSTEM_UNIT_DIR systemd systemd_system_unit_dir
DEFINE_VARIABLES prefix=${CMAKE_INSTALL_PREFIX})
else()
pkg_get_variable(SYSTEMD_PREFIX systemd prefix)
pkg_get_variable(SYSTEMD_SYSTEM_UNIT_DIR systemd systemd_system_unit_dir)
string(REPLACE "${SYSTEMD_PREFIX}" "${CMAKE_INSTALL_PREFIX}" SYSTEMD_SYSTEM_UNIT_DIR "${SYSTEMD_SYSTEM_UNIT_DIR}") endif() endif() pkg_get_variable(SYSTEMD_SYSTEM_UNIT_PATH systemd systemd_system_unit_path) if(NOT SYSTEMD_SYSTEM_UNIT_PATH MATCHES ".*:${SYSTEMD_SYSTEM_UNIT_DIR}:.*") message(WARNING SYSTEMD_SYSTEM_UNIT_DIR="${SYSTEMD_SYSTEM_UNIT_DIR}" is not in the system unit search path of current systemd installation) endif() install(FILES path/to/your.service DESTINATION ${SYSTEMD_SYSTEM_UNIT_DIR}) ``` - systemd user-level unit ```cmake find_package(PkgConfig) if(NOT SYSTEMD_USER_UNIT_DIR) if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.28") pkg_get_variable(SYSTEMD_USER_UNIT_DIR systemd systemd_user_unit_dir DEFINE_VARIABLES prefix=${CMAKE_INSTALL_PREFIX}) else() pkg_get_variable(SYSTEMD_PREFIX systemd prefix) pkg_get_variable(SYSTEMD_USER_UNIT_DIR systemd systemd_user_unit_dir) string(REPLACE "${SYSTEMD_PREFIX}" "${CMAKE_INSTALL_PREFIX}" SYSTEMD_USER_UNIT_DIR "${SYSTEMD_USER_UNIT_DIR}") endif() endif() pkg_get_variable(SYSTEMD_USER_UNIT_PATH systemd systemd_user_unit_path) if(NOT SYSTEMD_USER_UNIT_PATH MATCHES ".*:${SYSTEMD_USER_UNIT_DIR}:.*") message(WARNING SYSTEMD_USER_UNIT_DIR="${SYSTEMD_USER_UNIT_DIR}" is not in the user unit search path of current systemd installation) endif() install(FILES path/to/your.service DESTINATION ${SYSTEMD_USER_UNIT_DIR}) ``` - desktop file ```cmake install(FILES path/to/your.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications) ``` - icon corresponding to desktop file ```cmake install(FILES path/to/your.png
DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/48x48/apps)
# Add more size of .png icons here...

install(FILES path/to/your.svg
DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
````

### Configuration file

#### desktop file

It is **not** recommended\*\* to have `-` in the desktop file name. After removing the .desktop suffix, it should comply with the relevant specifications described in the application name section.

- **Recommended** Fill in the [`TryExec` field][key-tryexec] to ensure that the desktop file is no longer valid after the application has been uninstalled
- **Recommended** Fill in the [`WMClass` field][key-startupwmclass] to ensure that basic desktop environment functions such as the taskbar based on window and application matching can work properly
- **Recommended** Use only the executable file name instead of the absolute path when filling in the [`Exec` field][key-exec]
- **Not recommended** Use the absolute path when filling in the [`Icon` field][key-icon]

[key-tryexec]: https://specifications.freedesktop.org/desktop-entry-spec/latest/recognized-keys.html#key-tryexec
[key-startupwmclass]: https://specifications.freedesktop.org/desktop-entry-spec/latest/recognized-keys.html#key-startupwmclass

[key-exec]: https://specifications.freedesktop.org/desktop-entry-spec/latest/recognized-keys.html#key-exec [key-icon]: https://specifications.freedesktop.org/desktop-entry-spec/

latest/recognized-keys.html#key-icon

#### DBus service

- **Recommend** that the file name of the service file is consistent with the Name field in the file
- **Recommend** that the absolute path is used in the Exec field in the service file
- **Recommend** that the SystemdService field is filled in
- **Recommend** that the service name in the SystemdService field is consistent with the Name field
- **Recommend** that the Name field ends with a number

#### Systemd service

- **Recommend** that the file name of the service file with BusName is consistent with BusName
- **Recommend** that the absolute path is used in the ExecStart field

When using absolute paths in the above configuration files, **hard-coded paths are **not recommended**. The path **should** be consistent with the final installation path. **Recommend\*\* that the template file be written in the project first, and the placeholder is used to represent the absolute path. The final configuration file is generated by replacing the placeholder through the build system.

Here, taking the desktop file as an example, several examples of generating the final configuration file under common build systems are given.
Assume that the final product `org.deepin.demo.desktop` has the following content:

```ini
[Desktop Entry]
Name=demo
Exec=/usr/bin/demo
Type=Application
Terminal=false
```

- Use Makefile as the build system.

1. First write the template file `org.deepin.demo.desktop.in`, the content is as follows:

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

# Replace placeholders and generate final .desktop file
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

- If using CMake as build system.

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
# Use configure_file to replace placeholders
configure_file(
${CMAKE_CURRENT_SOURCE_DIR}/org.deepin.demo.desktop.in
${CMAKE_CURRENT_BINARY_DIR}/${DESKTOP_FILE}
@ONLY
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${DESKTOP_FILE}
DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
```

### Header files and link libraries

Linglong's environment consists of up to three parts. Taking the compilation of `org.deepin.demo` under the `x86_64` architecture as an example, the default search path for header files and library files includes the following parts:

| **Composition**    | **Package name**       | **Header file**                         | **Library files**                                                                           |
| ------------------ | ---------------------- | --------------------------------------- | ------------------------------------------------------------------------------------------- |
| base               | org.deepin.base        | /usr/include                            | /usr/lib<br>/usr/lib/x86_64-linux-gnu                                                       |
| runtime (optional) | org.deepin.runtime.dtk | /runtime/include                        | /runtime/lib<br>/runtime/lib/x86_64-linux-gnu                                               |
| app                | org.deepin.demo        | /opt/apps/org.deepin.demo/files/include | /opt/apps/org.deepin.demo/files/lib<br>/opt/apps/org.deepin.demo/files/lib/x86_64-linux-gnu |

Priority is in order from top to bottom. If a header file exists in both `org.deepin.base` and `org.deepin.demo`, the file in `org.deepin.base` will be matched first when used. The same applies to library files.

The default search path is applicable to standard libraries or development libraries without configuration files. In actual build scenarios, development libraries usually provide configuration files to facilitate user compilation and linking. Developers should use this file in their own build system instead of relying on the default search path.

Common configuration files include `.pc`, `.cmake`, etc. How to use it depends on the development library and the build system. Here are some examples of using configuration files in common build systems.

#### Makefile

##### Use `.pc` file

```makefile
# Common variables, inherit the environment variable CXXFLAGS and append content
CXX = g++
CXXFLAGS = $(CXXFLAGS) -Wall -Wextra -std=c++11
# Get the content of the .pc file through the pkg-config tool
# The return value is generally -I/path -lname type content
PKG_CONFIG = pkg-config
LIBS = $(shell $(PKG_CONFIG) --cflags --libs libname)

TARGET = demo

SRCS = main.cpp

all: $(TARGET)

# Provide the information provided by .pc to the compiler during construction
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

# Add executable files
add_executable(demo main.cpp)

# Set the link library. The header file search path will be automatically expanded.
target_link_libraries(demo PRIVATE PkgConfig::PackageAliasName)
```

##### Using `.cmake` files

```cmake
find_package(<PackageName> REQUIRED COMPONENTS <Component>)

# Add executable files
add_executable(demo main.cpp)

# Set the link library. The header file search path will be automatically expanded.
target_link_libraries(demo PRIVATE PackageName::Component)
```

Note:

It is not recommended to hardcode the above default search paths directly in the project build configuration file.

It is not recommended to overwrite environment variables such as CFLAGS/CXXFLAGS. Additional parameters should be appended to these variables.

### Dependency introduction

As mentioned in the section `Header files and link libraries`, the Linglong environment consists of multiple parts. If the content in the base or even the runtime cannot meet the requirements, the developer should introduce the missing dependencies on the APP side. The introduction method is to declare it under the `sources` field in the `linglong.yaml` file and write the corresponding installation or compilation rules under the `build` field.

`sources` supports multiple file types, **allowing** to introduce source code or compiled binary files, even a deb. However, for the introduction of compiled binary files, developers must consider whether they are compatible with the current base or runtime.

#### Use source code to introduce

Introducing dependencies through source code is a **recommended** practice, which can greatly ensure the stability and maintainability of the build process. The disadvantage is that this may take developers a lot of time to write yaml files, because the dependencies may also have their own dependencies.

_If developers find that the dependencies are complex and repeatedly used by other applications, they should consider integrating the dependencies into a runtime type package. _

When the dependency is compiled in the Linglong environment, its configuration file is usually "reliable". After compilation and installation, developers can use it directly in the project.

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

#### Import using deb

This is a "shortcut". If the developer does not consider the subsequent updates of the application, this method can be used. The developer can use auxiliary tools to analyze the dependencies and import the dependencies involved in batches in sources.

##### Use vscode linglong extension

1. Install the aptly command line tool
2. Search for linglong in the vscode extension store and install related plug-ins.
3. Put the sources field at the end of linglong.yaml
4. Add the gen_deb_source comment at the end of sources
5. Press Ctrl+Shift+P to search and execute the linglong: Gen deb sources command

After execution, the yaml file will automatically write the following content:

```yaml
build:
# Unzip and import deb, install_dep file is automatically downloaded by the plugin
bash ./install_dep linglong/sources "$PREFIX"

# Compile and install APP
sources:
# Source code
- kind: git
url: # app source url
commit: # commit hash
# The following comments are generated and used by the plugin, retrieve libical-dev from the warehouse, analyze its dependencies, and automatically generate sources below the comments
# linglong:gen_deb_source sources amd64 https://ci.deepin.com/repo/deepin/deepin-community/backup/rc2 beige main
# linglong:gen_deb_source install libical-dev
```

Note:
The installation prefix of the deb compilation product is `/usr`, and the `install_dep` script will automatically process the `.pc` file and replace `/usr` with `$PREFIX`. `xxx.cmake` type files cannot be processed in batches, and may not work properly if there is hard-coded path behavior.

## Linglong

This section mainly describes some special requirements of the Linglong package management system for applications.

### Restrictions

- Applications that need to be packaged through the Linglong package management system must be run as the user who starts the Linglong container (usually a normal user). Any form of privilege escalation mechanism is not available when the Linglong package management system runs applications, including but not limited to the SUID bit recorded in the file system and privilege escalation mechanisms based on file system extension attributes such as capabilities.
- Linglong currently does not support packaging applications containing system-level systemd units.
- It is **not recommended** to package desktop environment components such as launchers, file managers, resource managers, status bars, etc.

### Specifications

#### Application package name

The package name of Linglong application **must** fully comply with the relevant provisions in the "Application Name" section above, except:

- Application names are case-insensitive

#### Application version number

The application version number of Linglong package **must** be four groups of decimal numbers separated by `.`, for example, `1.0.0.0`.

If the specified version number is less than four groups, 0 will be added to the back until the conditions are met, for example, `1.90` will be automatically added to `1.90.0.0`.

The total length of the version number after automatically adding to four groups in the string sense **must** not exceed 256 bytes.

#### Running environment

Linglong application **must** select a base as the basic running environment. Available base:

| **Base library** | **Package name/version** |
| ---------------- | ------------------------ |
| glibc(2.38)      | org.deepin.base/23.1.0.0 |

If you need to use additional frameworks other than the base environment, you should use the appropriate runtime. Available runtime:

| **Framework**       | **Package name/version**        |
| ------------------- | ------------------------------- |
| QT(5.15) + DTK(5.6) | org.deepin.runtime.dtk/23.1.0.0 |

When using base or runtime, it is recommended to fill in the first three digits of the version number, such as '23.1.0', to facilitate subsequent updates. Filling in the full 4-digit version means that base or runtime updates are prohibited.

#### Installation location

During the build and installation process, Linglong's build tool will set the `$PREFIX` environment variable. The value of this environment variable **should** be `/opt/apps/${APPID}/files`, but it is not recommended to use its common value directly during the build and installation process. **Recommend** to always read the `$PREFIX` environment variable.

For the application build and installation process, the only writable directories are

1. The `/source` directory where the source code required for the build is placed

2. The installation location, that is, the location specified by `$PREFIX`

Installing files to other locations during the build process is **prohibited**, which usually causes write failures or the application cannot find these files at runtime.

The following is an example of using the `$PREFIX` environment variable to control the application installation location when calling some common build systems:

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

## Build products

This section mainly describes the product composition and related functions after the application is successfully built.

### Directory structure

After the application is successfully built, Linglong's build tool will submit the product to the local cache. When distributing offline, it should be exported as a `.layer`
or `.uab` type file.

Build the product layer file by decompressing:

```bash
ll-builder extract org.deepin.demo_0.0.0.1_x86_64_binary.layer ./tmp
```

You can get the following directory structure:

```plain
./tmp
├── entries
│ └── share -> ../files/share
├── files
│ ├── bin
│ │ └── demo
│ ├── share
│ │ ├── applications
│ │ │ └── org.deepin.demo.desktop
│ │ ├── icons
│ │ │ └── hicolor
│ │ │ └── scalable
│ │ │ └── apps
│ │ │ └── org.deepin.demo.svg │ │ ├── doc │ │ │ ├── changelog.gz │ │ │ └── copyright │ │ ├── mime │ │ │ └── packages │ │ │ └── org.deepin.demo.xml │ │ ├── locale │ │ │ └── zh_CN │ │ │ └── info.json │ │ └── services │ │ └── org.deepin.demo.xml │ └── libs │ └── libdemo.so.5.2.1 ├── info.json └── org.deepin.demo.install
```

When the application is running, these files or directories will be mapped to the following paths in the container:

```plain
/
├── bin
├── ...
├── opt
│ └── apps
│ └── org.deepin.demo
│ ├── files
│ ├── entries
│ ├── info.json
│ └── org.deepin.demo.install
└── var
```

#### info.json文件

info.json is the application description file defined by Linglong. This file is automatically generated by the build tool and it **should** not be modified manually. Its content is as follows: ```json { "id": "org.deepin.demo", "arch": [ "x86_64" ], "base": "main:org.deepin.foundation/23.0.0/x86_64", "channel": "main", "command": [ "/opt/apps/org.deepin.demo/files/bin/demo" ], "description": "simple Qt demo.\n", "kind": "app", "module": "runtime", "name": "demo", "runtime": "main:org.deepin.Runtime/23.0.1/x86_64", "size": 118763, "version": "0.0.0.1"
}

````

The following is a description of each field in info.json:

`id`: package identifier, i.e., application package name.

`arch`: package support architecture, currently supports the following CPU architectures.

- `amd64`: Applicable to `x86_64` architecture `CPU`.

- `loongarch64`: Applicable to the new version of Loongson series `CPU`.

- `arm64`: Applicable to `ARM64` bit `CPU`.

`base`: The basic environment used by the package when running.

`channel`: Package distribution channel.

`command`: The default startup command of the package.

`description`: Description of the package.

`kind`: Package category.

`module`: Package module.

`name`: Common name of the package.

`runtime`: The environment used when the package runs.

`size`: Package size.

`version`: Application version, its format should meet the requirements described in the Application Version Number section.

#### entries directory

This directory is used to share application configuration files with the host (desktop environment). This directory usually has the following subdirectories:

- entries/share/applications
- entries/share/dbus-1/services
- entries/share/systemd/user
- entries/share/icons
- entries/share/mime
- entries/share/fonts

The entries directory is automatically generated by the build tool. There is only one soft link named share in the directory. The link file points to the files/share in the upper directory.

When the application is installed on the host, Linglong Package Manager will link all the files in it to the path added by Linglong to the `$XDG_DATA_DIRS` variable through the link file. That is, `/var/lib/linglong/entries/share/`.

```bash $ ls /var/lib/linglong/entries/share/applications/ -l lrwxrwxrwx 1 deepin-linglong deepin-linglong 101 July 30 11:13 org.deepin.demo.desktop -> ../../../layers/main/org.deepin.demo/0.0.0.1/x86_64/runtime/entries/share/applications/org.deep
in.demo.desktop
````

##### applications directory

Place the application startup configuration file, that is, the .desktop file.

```ini
[Desktop Entry]
Exec=demo
Name=demo
TryExec=demo
Type=Application
```

When building, this file will be automatically modified to:

```ini
[Desktop Entry]
Exec=/usr/bin/ll-cli run org.deepin.demo -- demo
Name=demo
TryExec=/usr/bin/ll-cli
Type=Application
```

An application can have multiple desktop files.

**Path correspondence:**

| **Packaging path**                                 | **Installation path**                                     |
| -------------------------------------------------- | --------------------------------------------------------- |
| $PREFIX/share/applications/org.deepin.demo.desktop | $XDG_DATA_DIRS/share/applications/org.deepin.demo.desktop |

##### dbus services directory

The dbus service directory registered by the program, for example:

```ini
[D-BUS Service]
Name=org.deepin.demo
Exec=/opt/apps/org.deepin.demo/files/bin/demo --dbus
```

When building, this file will be automatically modified to:

```ini
[D-BUS Service]
Name=org.deepin.demo
Exec=/usr/bin/ll-cli run org.deepin.demo -- /opt/apps/org.deepin.demo/files/bin/demo --dbus
```

An application can be configured with multiple services, and the service name must be a subdomain.

**Path correspondence:**

| **Packaging path**                                   | **Installation path**                                       |
| ---------------------------------------------------- | ----------------------------------------------------------- |
| $PREFIX/share/services/org.deepin.demo.service       | $XDG_DATA_DIRS/dbus-1/service/org.deepin.demo.service       |
| $PREFIX/share/services/org.deepin.demo.hello.service | $XDG_DATA_DIRS/dbus-1/service/org.deepin.demo.hello.service |

##### User-level systemd service

User-level service directory registered by the program, for example:

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

This file will be automatically modified to when it is built ：

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

| **Packaging path**                               | **Installation path**                               |
| ------------------------------------------------ | --------------------------------------------------- |
| $PREFIX/lib/systemd/user/org.deepin.demo.service | $XDG_DATA_DIRS/systemd/user/org.deepin.demo.service |

##### icons directory

The directory for storing application icons should be consistent with the system icons directory structure.

**Path correspondence:**

| **Packaging path**                                            | **Installation path**                                          |
| ------------------------------------------------------------- | -------------------------------------------------------------- |
| $PREFIX/share/icons/hicolor/scalable/apps/org.deepin.demo.svg | $XDG_DATA_DIRS/icons/hicolor/scalable/apps/org.deepin.demo.svg |
| $PREFIX/share/icons/hicolor/24x24/apps/org.deepin.demo.png    | $XDG_DATA_DIRS/icons/hicolor/24x24/apps/org.deepin.demo.png    |
| $PREFIX/share/icons/hicolor/16x16/apps/org.deepin.demo.png    | $XDG_DATA_DIRS/icons/hicolor/16x16/apps/org.deepin.demo.png    |

##### mime directory

MIME (Multipurpose Internet Mail Extensions) Multipurpose Internet Mail Extensions type. This directory is used to store mime configuration files. The files are in XML format and end with .xml.

**Path correspondence:**

| **Packaging path**                              | **Installation path**                            |
| ----------------------------------------------- | ------------------------------------------------ |
| $PREFIX/share/mime/packages/org.deepin.demo.xml | $XDG_DATA_DIRS/mime/packages/org.deepin.demo.xml |

##### fonts directory

Font storage path.

#### files directory

Store various files required by the application. There is no restriction on placing files in this directory, but it is recommended to place executable programs in the bin subdirectory. It is recommended that third-party libraries that applications or plug-ins depend on be placed in the /opt/apps/${id}/files/lib directory.

#### .install file

The org.deepin.demo.install file in the above example org.deepin.demo is a file automatically generated during the build process. This file is used to define which files should be installed in the binary module and can be used to trim the size of the final product of the software package.

When the file is not defined in the same directory as linglong.yaml, all content is installed to the binary module according to the rules under build in linglong.yaml.

Usage:

After the first successful build, the .install file will be generated in the product and record all files installed to the binary module. Copy the file to the same directory as linglong.yaml, modify the content in the .install file and build again. Only the content noted in .install will be submitted to the binary module.

## References

[Desktop Application Packaging Specifications](https://github.com/linuxdeepin/deepin-specifications/blob/master/unstable/%E6%A1%8C%E9%9D%A2%E5%BA%94%E7%94%A8%E6%89%93%E5%8C%85%E8%A7%84%E8%8C%83.md)
