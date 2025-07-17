# 玲珑应用应用程序打包规范

本文中的关键词**必须**、**禁止**、**必要的**、**应当**、**不应**、**推荐的**、**允许**以及**可选的**[^rfc2119-keywords]的解释见于[RFC 2119][rfc-2119]中的描述。

这些关键词与原文中的英语词汇的对应关系如下表所示：

| 中文       | 英语        |
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

本文档旨在帮助应用开发者规范应用程序的构建过程中的行为，以迁移到玲珑包管理系统中。

## 通用

本节主要记录一些通用的规范，几乎所有项目都应当遵守。

### 应用名称

应用的名称**应当**使用[反向域名表示法](https://zh.wikipedia.org/wiki/%E5%8F%8D%E5%90%91%E5%9F%9F%E5%90%8D%E8%A1%A8%E7%A4%BA%E6%B3%95)，例如`org.deepin.demo`。

该应用名称主要用于desktop文件的命名、DBus服务的命名等场景。

- 开发者**应当**使用自己确实拥有的域名的倒置作为应用名称的前缀，并在之后跟上应用程序名称

  **注意**：若开发者无法证明其确实拥有该域名，有可能导致应用包从仓库中移除。

- 对于github上开发的第三方应用而言，如果该应用程序所在的组织有额外的域名，则应当优先使用，否则应当采用`io.github.<GITHUB_ID>.`作为前缀。

  特别的，如果该应用的组织名和应用名称一致，例如<https://github.com/neovim/neovim>，打包者**不应当**省略重复的应用名称与组织名，这个应用的ID**应当**为`io.github.neovim.neovim`。

  **注意**：实际上该组织拥有域名`neovim.io`，故最合理的的应用名称**应当**为`io.neovim.neovim`。

- **不推荐**使用含有`-`的应用名称，如果域名/应用名称确实含有`-`，**推荐**使用`_`代替

- **不推荐**应用名称以`.desktop`结尾

以上规范来自[Desktop Entry Specification][desktop-entry-specification]。

[desktop-entry-specification]: https://specifications.freedesktop.org/desktop-entry-spec/latest

拓展阅读：<https://docs.flatpak.org/en/latest/conventions.html#application-ids>

### `prefix`与`$DESTDIR`

在编写应用程序的构建过程时，开发者**不应当**假设自己安装的位置是固定的。在Makefile/CMakeLists.txt中将可执行文件安装到硬编码的路径，例如`/usr/bin`是不规范的行为。

开发者编写构建/安装过程时**应当**尊重构建系统的`prefix`配置以及`$DESTDIR`环境变量的值以使得打包者可以方便地在打包时配置该应用程序的具体安装位置。

`prefix`指的是构建/安装时指定给构建系统的、应用最终会被安装到系统中的具体位置。

当开发者没有指定安装位置时，其默认值**应当**为`/usr/local`，
通过包管理系统打包时，包管理系统会配置其值。当使用dpkg相关工具进行打包时，其值会被配置为`/usr`。但编写构建/安装过程时，开发者应当考虑`prefix`被配置成任意值的情况。

`$DESTDIR`是指构建系统进行安装时，为了方便发行版打包等过程，约定的一个环境变量。其大致工作逻辑如下：

若构建系统完成了构建工作后，执行安装过程时，被指定了`prefix=/usr`，且`$DESTDIR=./tmp`，则其完成安装后，所有的产物文件都应当出现在`./tmp/usr`目录中。打包工具随后会将`./tmp`视为根目录将其中的文件进行压缩打包等工作。

这里以常见的构建系统为例，给出几种常见文件安装过程的**推荐**编写方式，这些编写方式均尊重`prefix`配置以及`$DESTDIR`：

#### Makefile

本节主要参考<https://www.gnu.org/prep/standards/standards.html>以及<https://wiki.debian.org/Multiarch/Implementation>中的相关内容编写。

这里定义一些变量的默认值以及其他公共部分以便后文编写示例，这些变量默认值的相关说明可以在上方链接中查找。

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

# 以上定义的这些具有默认值的变量均为上述规范中的推荐行为，不建议做任何修改。

PKG_CONFIG ?= pkg-config

.PHONY: install
```

- 可执行文件

  ```makefile
  install:
      $(INSTALL) -d "$(DESTDIR)$(bindir)"
      $(INSTALL_PROGRAM) path/to/your/executable "$(DESTDIR)$(bindir)"/executable
  ```

- 内部可执行文件

  指不应当由用户在终端中直接调用的可执行文件，这些可执行文件**不应当**可以通过`$PATH`找到

  ```makefile
  install:
      $(INSTALL) -d "$(DESTDIR)$(libexecdir)"
      $(INSTALL_PROGRAM) path/to/my/internal/executable "$(DESTDIR)$(libexecdir)"/executable
  ```

- 静态库

  ```makefile
  install:
      $(INSTALL) -d "$(DESTDIR)$(libdir)"
      $(INSTALL_DATA) path/to/my/library.a "$(DESTDIR)$(libdir)"/library.a
  ```

- pkg-config配置文件

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

  若可以确定该文件跨架构可用，则可以使用`$(datarootdir)`代替`$(libdir)`。

- systemd系统级单元

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

- systemd用户级单元

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

- desktop文件

  ```makefile
  install:
      $(INSTALL) -d "$(DESTDIR)$(datarootdir)"/applications
      $(INSTALL_PROGRAM) path/to/your.desktop "$(DESTDIR)$(datarootdir)"/applications/your.desktop
  ```

- desktop文件对应图标

  参见：<https://specifications.freedesktop.org/icon-theme-spec/latest/#install_icons>
  - 如果安装的图标为固定大小的版本，那么**推荐**使用png格式

    至少**需要**安装一个48x48大小的png才能保证桌面环境中图标相关的基础功能正常

  - 如果安装的图标为矢量版本，那么**推荐**使用svg格式

  ```makefile
  install:
      $(INSTALL) -d "$(DESTDIR)$(datarootdir)"/icons/hicolor/48x48/apps
      $(INSTALL_DATA) path/to/your.png "$(DESTDIR)$(datarootdir)"/icons/hicolor/48x48/apps/your.png
      # Add more size of .png icons here...

      $(INSTALL) -d "$(DESTDIR)$(datarootdir)"/icons/hicolor/scalable/apps
      $(INSTALL_DATA) path/to/your.svg "$(DESTDIR)$(datarootdir)"/icons/hicolor/scalable/apps/your.svg
  ```

#### CMake

本节主要参考<https://cmake.org/cmake/help/v3.30/module/GNUInstallDirs.html#module:GNUInstallDirs>以及<https://wiki.debian.org/Multiarch/Implementation>中的相关内容编写。

这里定义一些变量的默认值以及其他公共部分以便后文编写示例，这些变量默认值的相关说明可以在上方链接中查找。

编写逻辑与Makefile一节中的相关内容一致。

```cmake
include(GNUInstallDirs)
```

- 可执行文件

  参见：<https://cmake.org/cmake/help/v3.30/command/install.html#programs>

  ```cmake
  install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/path/to/your/executable TYPE BIN)
  ```

- 内部可执行文件

  指不应当在终端中直接调用的可执行文件，这些可执行文件**不应当**可以通过`$PATH`找到

  参见：<https://cmake.org/cmake/help/v3.30/command/install.html#programs>

  ```cmake
  install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/path/to/your/executable DESTINATION ${CMAKE_INSTALL_LIBEXECDIR})
  ```

  注意：不使用TYPE是因为TYPE目前不支持LIBEXEC

- 目标文件/导出文件

  待补充

- pkg-config配置文件

  待补充

- systemd系统级单元

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

- systemd用户级单元

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

- desktop文件

  ```cmake
  install(FILES path/to/your.desktop
          DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
  ```

- desktop文件对应图标

  ```cmake
  install(FILES path/to/your.png
          DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/48x48/apps)
  # Add more size of .png icons here...

  install(FILES path/to/your.svg
          DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
  ```

### 配置文件

#### desktop文件

desktop文件的文件名中**不推荐**带有`-`，去掉.desktop后缀后，应当符合应用名称一节中描述的相关规范。

- **推荐**填写[`TryExec`字段][key-tryexec]，以确保应用已经卸载后，desktop文件不再有效
- **推荐**填写[`WMClass`字段][key-startupwmclass]，以确保任务栏等基于窗口和应用程序匹配的桌面环境基础功能能正常工作
- **推荐**填写[`Exec`字段][key-exec]时仅使用可执行文件名称而不是绝对路径
- **不推荐**填写[`Icon`字段][key-icon]时使用绝对路径

[key-tryexec]: https://specifications.freedesktop.org/desktop-entry-spec/latest/recognized-keys.html#key-tryexec
[key-startupwmclass]: https://specifications.freedesktop.org/desktop-entry-spec/latest/recognized-keys.html#key-startupwmclass
[key-exec]: https://specifications.freedesktop.org/desktop-entry-spec/latest/recognized-keys.html#key-exec
[key-icon]: https://specifications.freedesktop.org/desktop-entry-spec/latest/recognized-keys.html#key-icon

#### DBus服务

- **推荐**service文件的文件名与文件中的Name字段一致
- **推荐**service文件中的Exec字段中使用绝对路径
- **推荐**填写SystemdService字段
- **推荐**SystemdService字段中的服务名称与Name字段一致
- **推荐**Name字段以数字结尾

#### Systemd服务

- **推荐**具有BusName的service文件的文件名与BusName一致
- **推荐**ExecStart字段中使用绝对路径

上述配置文件在使用绝对路径时，**不推荐**硬编码路径。其路径**应当**与最终安装路径保持一致。**推荐**在项目中先编写模板文件，使用占位符表示绝对路径，通过构建系统替换占位符后生成最终配置文件。

这里以desktop文件为例，给出几个常见构建系统下生成最终配置文件的例子。
假设最终产物`org.deepin.demo.desktop`内容如下：

```ini
[Desktop Entry]
Name=demo
Exec=/usr/bin/demo
Type=Application
Terminal=false
```

- 使用Makefile作为构建系统。

  1、先编写模板文件`org.deepin.demo.desktop.in`，内容如下：

  ```ini
  [Desktop Entry]
  Name=demo
  Exec=@BINDIR@/demo
  Type=Application
  Terminal=false
  ```

  2、编写对应的makefile规则。

  ```makefile
  DESKTOP_TEMPLATE = org.deepin.demo.desktop.in
  DESKTOP_FILE = org.deepin.demo.desktop

  # 替换占位符并生成最终的 .desktop 文件
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

- 如果使用CMake作为构建系统。
  1、编写desktop模板文件。

  ```desktop
  [Desktop Entry]
  Name=demo
  Exec=@CMAKE_INSTALL_BINDIR@/demo
  Type=Application
  Terminal=false
  ```

  2、编写对应的cmake规则。

  ```cmake
  set(DESKTOP_FILE "org.deepin.demo.desktop")
  # 使用 configure_file 进行占位符替换
  configure_file(
      ${CMAKE_CURRENT_SOURCE_DIR}/org.deepin.demo.desktop.in
      ${CMAKE_CURRENT_BINARY_DIR}/${DESKTOP_FILE}
      @ONLY
  )
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${DESKTOP_FILE}
          DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
  ```

### 头文件以及链接库

玲珑的环境最多有三部分组成，以在`x86_64`架构下编译`org.deepin.demo`为例，其头文件以及库文件默认搜索路径包含以下部分：

| **组成**       | **包名**               | **头文件**                              | **库文件**                                                                                  |
| -------------- | ---------------------- | --------------------------------------- | ------------------------------------------------------------------------------------------- |
| base           | org.deepin.base        | /usr/include                            | /usr/lib<br>/usr/lib/x86_64-linux-gnu                                                       |
| runtime (可选) | org.deepin.runtime.dtk | /runtime/include                        | /runtime/lib<br>/runtime/lib/x86_64-linux-gnu                                               |
| app            | org.deepin.demo        | /opt/apps/org.deepin.demo/files/include | /opt/apps/org.deepin.demo/files/lib<br>/opt/apps/org.deepin.demo/files/lib/x86_64-linux-gnu |

优先级按从上往下的顺序排列。如果一份头文件同时在`org.deepin.base`和`org.deepin.demo`中存在，使用时会优先匹配到`org.deepin.base`中的文件。库文件同理。

默认搜索路径适用于标准库或者不带配置文件的开发库。在实际构建场景中，开发库通常会提供配置文件以方便使用者编译和链接，开发者**应当**在自己的构建系统使用该文件而不是依赖默认搜索路径。

常见的配置文件有`.pc`、`.cmake`等。具体如何使用取决于开发库以及构建系统，这里给出几种常用的构建系统使用配置文件的例子。

#### Makefile

##### 使用`.pc`文件

```makefile
# 常用变量, 继承环境变量CXXFLAGS并追加内容
CXX = g++
CXXFLAGS = $(CXXFLAGS) -Wall -Wextra -std=c++11
# 通过pkg-config工具获取.pc文件内容
# 返回值一般为 -I/path -lname 类型的内容
PKG_CONFIG = pkg-config
LIBS = $(shell $(PKG_CONFIG) --cflags --libs libname)

TARGET = demo

SRCS = main.cpp

all: $(TARGET)

# 构建时将.pc提供的信息提供给编译器
$(TARGET): $(SRCS)
 $(CXX) $(CXXFLAGS) $(LIBS) -o $@ $^

clean:
 rm -f $(TARGET)
```

#### CMake

##### 使用`.pc`文件

```cmake
find_package(PkgConfig REQUIRED)
pkg_check_modules(PackageAliasName REQUIRED IMPORTED_TARGET PackageName)

# 添加可执行文件
add_executable(demo main.cpp)

# 设置链接库。头文件搜索路径将自动展开。
target_link_libraries(demo PRIVATE PkgConfig::PackageAliasName)
```

##### 使用`.cmake`文件

```cmake
find_package(<PackageName> REQUIRED COMPONENTS <Component>)

# 添加可执行文件
add_executable(demo main.cpp)

# 设置链接库。头文件搜索路径将自动展开。
target_link_libraries(demo PRIVATE PackageName::Component)
```

注意:

**不推荐**直接在项目构建配置文件中硬编码上述默认搜索路径。
**不推荐**覆盖CFLAGS/CXXFLAGS等环境量，有额外参数**应当**向这些变量后追加。

### 依赖引入

在`头文件及链接库`一节提到，玲珑环境由多个部分组成。如果base甚至runtime中的内容无法满足要求时，开发者**应当**自行在APP一侧引入缺失的依赖。引入方式为，在`linglong.yaml`文件中`sources`字段下声明，并在`build`字段下编写相应的安装或者编译规则。

`sources`支持多种文件类型，**允许**引入源码或者编译好的二进制文件，甚至一个deb。但对于引入编译好的二进制文件，开发者**必须**考虑其是否与当前base或runtime兼容。

#### 使用源码引入

通过源码引入依赖是一个**推荐的**做法，它能极大的保证构建流程的稳定以及可维护性。缺点是这可能会花费开发者不少时间编写yaml文件，因为依赖也许也会有自身的依赖。

_如果开发者发现依赖复杂且重复被其他应用使用，那么应当考虑将依赖整合做成一个runtime类型的包。_

当依赖是在玲珑环境下编译产生时，其配置文件通常是“可靠的”。编译安装后开发者可以直接在项目中使用。

```yaml
sources:
  - kind: git
    url: # app source url
    commit: # commit hash
  - kind: git
    url: # dependency source url
    commit: # commit hash
build:
  # 编译依赖
  cd /project/your_dependency_name/
  cmake -Bbuild -DCMAKE_INSTALL_PREFIX=$PREFIX
  make install

  # 编译、安装APP
  cd /project/your_app_name
  cmake -Bbuild -DCMAKE_INSTALL_PREFIX=$PREFIX
  make install
```

#### 使用deb引入

这是一条“捷径”。如果开发者不考虑应用的后续更新可使用此方式。开发者可以借助辅助工具分析依赖关系，将涉及到的依赖批量在sources中导入。

##### 使用vscode linglong扩展

1. 安装 aptly 命令行工具
2. 在 vscode 扩展商店搜索 linglong 安装相关插件。
3. 将 sources 字段放在 linglong.yaml 的最后
4. 在 sources 最后添加 gen_deb_source 注释
5. 按 Ctrl+Shift+P 搜索执行 linglong: Gen deb sources 命令

执行后的yaml文件将自动写入如下内容：

```yaml
build:
  # 解压导入deb，install_dep文件由插件自动下载
  bash ./install_dep linglong/sources "$PREFIX"

  # 编译、安装APP
sources:
  # 源码
  - kind: git
    url: # app source url
    commit: # commit hash
  # 以下注释由插件生成并使用，从仓库中检索 libical-dev 分析其依赖，自动生成sources到注释的下面
  # linglong:gen_deb_source sources amd64 https://ci.deepin.com/repo/deepin/deepin-community/backup/rc2 beige main
  # linglong:gen_deb_source install libical-dev
```

注意：
deb的编译产物,安装前缀是`/usr`，`install_dep`脚本会自动处理其中的`.pc`文件，将`/usr`替换成`$PREFIX`。`xxx.cmake`类型文件无法批量处理，如果有硬编码路径行为，可能无法正常工作。

## 玲珑

本节主要描述一些玲珑包管理系统对应用程序的特殊要求。

### 限制

- 需要通过玲珑包管理系统打包的应用程序**必须**以启动玲珑容器的用户身份（通常为普通用户）运行。任何形式的权限提升机制在玲珑包管理系统运行应用时均不可用，包括但不限于文件系统中记录的SUID位以及capabilities等基于文件系统拓展属性的权限提升机制。
- 玲珑目前不支持打包含有系统级systemd单元的应用程序。
- **不推荐**打包桌面环境（Desktop Environment）组件，如启动器、文件管理器、资源管理器、状态栏等。

### 规范

#### 应用包名

玲珑应用的包名**必须**完全符合上文“应用名称”一节中的相关规定，除此之外：

- 应用名称不区分大小写

#### 应用版本号

玲珑包的应用版本号**必须**为`.`分割的四组十进制数字，例如`1.0.0.0`。

如果指定的版本号不足四组，则会向后补充0直至满足条件，例如`1.90`会被自动补充为`1.90.0.0`。

自动补足至四组后的版本号总体长度在字符串意义上**禁止**超过256字节。

#### 运行环境

玲珑应用**必须**选择一个base作为基础运行环境。可使用的base:

| **基础库**  | **包名/版本**            |
| ----------- | ------------------------ |
| glibc(2.38) | org.deepin.base/23.1.0.0 |

如果需要额外使用基础环境以外的框架，**应当**使用合适的runtime。可使用的runtime：

| **框架**            | **包名/版本**                   |
| ------------------- | ------------------------------- |
| QT(5.15) + DTK(5.6) | org.deepin.runtime.dtk/23.1.0.0 |

在使用base或runtime时，版本号**推荐**填写前三位，如 '23.1.0'，便于后续接收更新。全量填写4位版本表示**禁止**base或runtime更新。

#### 安装位置

在构建以及安装过程中过程中，玲珑的构建工具会设置`$PREFIX`环境变量。该环境变量的值**应当**为`/opt/apps/${APPID}/files`，但并不建议在编写构建以及安装过程中直接使用其常见值，**推荐**总是读取`$PREFIX`环境变量。

对应用程序的构建以及安装过程而言，可写的目录仅有

1. 放置构建所需源码的`/source`目录
2. 安装的位置，即`$PREFIX`指定的位置

向构建过程中的其他位置安装文件是**禁止**的，通常会导致写失败或应用在运行时无法找到这些文件。

以下是在调用部分常见构建系统时使用`$PREFIX`环境变量控制应用安装位置的例子:

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

## 构建产物

本节主要描述应用构建成功后的产物组成及相关功能。

### 目录结构

玲珑的构建工具在应用构建成功后，会将产物提交到本地的缓存中。在进行离线分发时，应该导出成`.layer`
或`.uab`类型文件。

通过解压构建产物layer文件：

```bash
ll-builder extract org.deepin.demo_0.0.0.1_x86_64_binary.layer ./tmp
```

可以得到以下目录结构：

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

应用运行时，这些文件或目录将被映射到容器中如下路径：

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

#### info.json文件

info.json是玲珑定义的应用描述文件。该文件由构建工具自动生成，它**不应**被手动修改。其内容如下：

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

下面对info.json中各个字段的依次说明：

`id`：软件包标识，即应用包名。

`arch`：软件包支持架构，当前支持如下 CPU 架构。

- `amd64`：适用 `x86_64` 架构 `CPU`。
- `loongarch64`：适用新版龙芯系列 `CPU`。
- `arm64`：适用 `ARM64` 位 `CPU`。

`base`: 软件包运行时使用的基础环境。

`channel`：软件包分发渠道。

`command`: 软件包默认启动命令。

`description`：软件包的描述信息。

`kind`：软件包类别。

`module`：软件包模块。

`name`：软件包常用名。

`runtime`：软件包运行时使用的环境。

`size`：软件包大小。

`version`：应用版本，其格式应满足应用版本号一节所述要求。

#### entries 目录

该目录用于共享应用配置文件给宿主机（桌面环境），该目录下通常有如下子目录：

- entries/share/applications
- entries/share/dbus-1/services
- entries/share/systemd/user
- entries/share/icons
- entries/share/mime
- entries/share/fonts

entries目录由构建工具自动生成，目录下只存放了一个名为share的软链接，该链接文件指向上层目录的files/share。

当应用被安装到宿主机上时，玲珑包管理器会通过该链接文件将其中的所有文件链接到玲珑添加到`$XDG_DATA_DIRS`变量中的路径下。即`/var/lib/linglong/entries/share/`。

```bash
$ ls /var/lib/linglong/entries/share/applications/ -l
lrwxrwxrwx 1 deepin-linglong deepin-linglong 101 7月30日 11:13 org.deepin.demo.desktop -> ../../../layers/main/org.deepin.demo/0.0.0.1/x86_64/runtime/entries/share/applications/org.deepin.demo.desktop
```

##### applications目录

放置应用启动配置文件，即.desktop文件。

```ini
[Desktop Entry]
Exec=demo
Name=demo
TryExec=demo
Type=Application
```

该文件在构建时，将会被自动修改为 ：

```ini
[Desktop Entry]
Exec=/usr/bin/ll-cli run org.deepin.demo -- demo
Name=demo
TryExec=/usr/bin/ll-cli
Type=Application
```

应用可以有多个desktop文件。

**路径对应关系：**

| **打包路径**                                       | **安装路径**                                              |
| -------------------------------------------------- | --------------------------------------------------------- |
| $PREFIX/share/applications/org.deepin.demo.desktop | $XDG_DATA_DIRS/share/applications/org.deepin.demo.desktop |

##### dbus services 目录

程序注册的 dbus 服务目录，例如:

```ini
[D-BUS Service]
Name=org.deepin.demo
Exec=/opt/apps/org.deepin.demo/files/bin/demo --dbus
```

该文件在构建时，将会被自动修改为 ：

```ini
[D-BUS Service]
Name=org.deepin.demo
Exec=/usr/bin/ll-cli run org.deepin.demo -- /opt/apps/org.deepin.demo/files/bin/demo --dbus
```

一个应用允许配置多个 service，服务名必须是子域名。

**路径对应关系：**

| **打包路径**                                         | **安装路径**                                                |
| ---------------------------------------------------- | ----------------------------------------------------------- |
| $PREFIX/share/services/org.deepin.demo.service       | $XDG_DATA_DIRS/dbus-1/service/org.deepin.demo.service       |
| $PREFIX/share/services/org.deepin.demo.hello.service | $XDG_DATA_DIRS/dbus-1/service/org.deepin.demo.hello.service |

##### 用户级systemd服务

程序注册的用户级服务目录，例如:

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

该文件在构建时，将会被自动修改为 ：

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

与dbus service不同的是，安装到`$PREFIX/lib/systemd/user`下的文件会被自动拷贝到`$PREFIX/share/systemd/user`。

**路径对应关系：**

| **打包路径**                                     | **安装路径**                                        |
| ------------------------------------------------ | --------------------------------------------------- |
| $PREFIX/lib/systemd/user/org.deepin.demo.service | $XDG_DATA_DIRS/systemd/user/org.deepin.demo.service |

##### icons 目录

应用图标存放目录，结构与系统 icons 目录结构保持一致即可。

**路径对应关系：**

| **打包路径**                                                  | **安装路径**                                                   |
| ------------------------------------------------------------- | -------------------------------------------------------------- |
| $PREFIX/share/icons/hicolor/scalable/apps/org.deepin.demo.svg | $XDG_DATA_DIRS/icons/hicolor/scalable/apps/org.deepin.demo.svg |
| $PREFIX/share/icons/hicolor/24x24/apps/org.deepin.demo.png    | $XDG_DATA_DIRS/icons/hicolor/24x24/apps/org.deepin.demo.png    |
| $PREFIX/share/icons/hicolor/16x16/apps/org.deepin.demo.png    | $XDG_DATA_DIRS/icons/hicolor/16x16/apps/org.deepin.demo.png    |

##### mime 目录

MIME(Multipurpose Internet Mail Extensions)多用途互联网邮件扩展类型。该目录用于存放 mime 配置文件，文件是 XML 格式,以.xml 结尾。

**路径对应关系：**

| **打包路径**                                    | **安装路径**                                     |
| ----------------------------------------------- | ------------------------------------------------ |
| $PREFIX/share/mime/packages/org.deepin.demo.xml | $XDG_DATA_DIRS/mime/packages/org.deepin.demo.xml |

##### fonts 目录

字体存放路径。

#### files 目录

存放应用程序需要的各种文件，对于该目录放置文件并无限制，但是建议将可执行程序放置到 bin 子目录。应用程序或者插件依赖的第三方库推荐放置在/opt/apps/${id}/files/lib 目录。

#### .install文件

上述示例org.deepin.demo中的org.deepin.demo.install文件是构建过程中自动生成的文件。该文件用于定义binary模块中应该安装哪些文件，可用于软件包最终产物体积裁剪。

未在linglong.yaml同级目录下定义该文件时，所有内容均按linglong.yaml中build下的规则安装到binary模块。

使用方法：

第一次构建成功后，会在产物中生成.install文件并记录所有安装至binary模块的文件。将该文件拷贝到linglong.yaml同级的目录下，修改.install文件中的内容后再次构建，只有.install内注明的内容会被提交到binary模块。

## 参考资料

[桌面应用打包规范](https://github.com/linuxdeepin/deepin-specifications/blob/master/unstable/%E6%A1%8C%E9%9D%A2%E5%BA%94%E7%94%A8%E6%89%93%E5%8C%85%E8%A7%84%E8%8C%83.md)
