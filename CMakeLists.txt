# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

cmake_minimum_required(VERSION 3.11.4)

project(
  linglong
  VERSION 1.10.0
  DESCRIPTION "a container based application package manager for Linux desktop"
  HOMEPAGE_URL "https://github.com/OpenAtom-Linyaps/linyaps"
  LANGUAGES CXX C)

set(LINGLONG_VERSION
    ""
    CACHE STRING "The version of linglong project.")

if(NOT ("${LINGLONG_VERSION}" STREQUAL ""))
  message(STATUS "Project version has been overridden to ${LINGLONG_VERSION}")
  set(PROJECT_VERSION ${LINGLONG_VERSION})
endif()

set(ENABLE_LINGLONG_APP_BUILDER_UTILS
    OFF
    CACHE BOOL "enable build ll-builder-utils to linglong app")

set(BUILD_LINGLONG_BUILDER_UTILS_IN_BOX
    OFF
    CACHE BOOL "build ll-builder-utils in box")

set(ENABLE_LINGLONG_INSTALLER
    OFF
    CACHE BOOL "enable linglong installer")

set(ENABLE_TESTING
    ON
    CACHE BOOL "enable testing")

set(LINGLONG_USERNAME
    "deepin-linglong"
    CACHE STRING "The username for linglong package manager")
set(LINGLONG_ROOT
    "/var/lib/linglong"
    CACHE STRING "The location where linglong related program puts their data")
set(LINGLONG_DEFAULT_OCI_RUNTIME
    "ll-box"
    CACHE STRING "The oci runtime which linglong use by default")

set(LINGLONG_CLI_BIN
    "ll-cli"
    CACHE STRING "The client of package management.")
set(GETTEXT_DOMAIN_NAME
    "linyaps"
    CACHE STRING "The name of gettext domain.")
set(QT_VERSION_MAJOR CACHE STRING "The major version of qt.")
set(LINGLONG_EXPORT_PATH
    "share"
    CACHE STRING
          "The path to export linglong config files, it must end with share.")

if(NOT LINGLONG_EXPORT_PATH MATCHES ".*share$")
  message(
    WARNING
      "LINGLONG_EXPORT_PATH '${LINGLONG_EXPORT_PATH}' must end with 'share'")
  set(LINGLONG_EXPORT_PATH "share")
endif()

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(PFL)

set(ENABLE_CPM YES)

if(CMAKE_VERSION VERSION_LESS "3.14")
  set(ENABLE_CPM NO)
  message(
    STATUS "cmake version ${CMAKE_VERSION} not compatible with CPM.cmake.")
endif()

if(CPM_LOCAL_PACKAGES_ONLY)
  set(ENABLE_CPM NO)
endif()

if(ENABLE_CPM)
  include(CPM)
  CPMFindPackage(
    NAME tl-expected
    VERSION 1.0.0 # NOTE: Upstream cmake version doesn't match git tag, we need
                  # https://github.com/TartanLlama/expected/pull/62 in v1.1.0,
                  # but cmake version of v1.1.0 is 1.0.0.
    GITHUB_REPOSITORY TartanLlama/expected
    GIT_TAG v1.1.0
    GIT_SHALLOW ON
    OPTIONS "EXPECTED_BUILD_TESTS OFF"
    EXCLUDE_FROM_ALL ON)
  CPMFindPackage(
    NAME yaml-cpp
    VERSION 0.6.2
    GITHUB_REPOSITORY jbeder/yaml-cpp
    GIT_TAG 0.8.0 # NOTE: When use this project with CPM.cmake(FetchContent), we
                  # need https://github.com/jbeder/yaml-cpp/pull/583 in v0.8.0
    EXCLUDE_FROM_ALL ON
    OPTIONS "YAML_CPP_INSTALL ON" "YAML_CPP_BUILD_TESTS OFF")
  CPMFindPackage(
    NAME nlohmann_json
    VERSION 3.5.0
    URL "https://github.com/nlohmann/json/archive/refs/tags/v3.5.0.tar.gz"
    EXCLUDE_FROM_ALL ON
    OPTIONS "JSON_BuildTests OFF")
  CPMFindPackage(
    NAME CLI11
    VERSION 2.4.1
    GITHUB_REPOSITORY CLIUtils/CLI11
    GIT_TAG v2.4.1
    EXCLUDE_FROM_ALL ON
    OPTIONS "CLI11_BUILD_TESTS OFF")

  if(ENABLE_TESTING)
    include(CTest)
    CPMFindPackage(
      NAME googletest
      GITHUB_REPOSITORY google/googletest
      GIT_TAG v1.14.0
      VERSION 1.12.1
      OPTIONS "INSTALL_GTEST OFF" "gtest_force_shared_crt"
      FIND_PACKAGE_ARGUMENTS "NAMES GTest"
      GIT_SHALLOW ON
      EXCLUDE_FROM_ALL ON)
  endif()
endif()

set(linglong_EXTERNALS "ytj ytj::ytj")

if(CMAKE_SYSTEM_PROCESSOR MATCHES "^mips" OR CMAKE_SYSTEM_PROCESSOR MATCHES
                                             "^MIPS")
  message(
    STATUS
      "MIPS architecture detected. Globally adding -mxgot to CMAKE_CXX_FLAGS.")

  set(CMAKE_CXX_FLAGS
      "${CMAKE_CXX_FLAGS} -mxgot"
      CACHE STRING "C++ compiler flags" FORCE)
endif()

# NOTE: UOS v20 do not have tl-expected packaged.
find_package(tl-expected 1.0.0 QUIET)
if(NOT tl-expected_FOUND)
  # list(APPEND linglong_EXTERNALS tl-expected) # tl-expected requires 3.14
  add_library(tl-expected INTERFACE)
  add_library(tl::expected ALIAS tl-expected)
  target_include_directories(tl-expected
                             INTERFACE ./external/tl-expected/include)
  list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake.external)
endif()

find_package(nlohmann_json 3.5.0 QUIET)
if(NOT nlohmann_json_FOUND)
  list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake.fix)
  find_package(nlohmann_json 3.5.0 REQUIRED)
endif()

# NOTE: UOS v20 do not have cli11 packaged.
find_package(CLI11 2.4.1 QUIET)
if(NOT CLI11_FOUND)
  list(APPEND linglong_EXTERNALS "CLI11 CLI11::CLI11")
endif()

find_package(PkgConfig REQUIRED)

if(ENABLE_TESTING)
  pkg_search_module(CRYPTO REQUIRED IMPORTED_TARGET libcrypto)
  find_package(GTest REQUIRED)

  # compatible with old gtest
  if(NOT TARGET GTest::gmock)
    message(STATUS "GTest::gmock not found, alias GTest::gtest")
    add_library(GTest::gmock ALIAS GTest::GTest) # available since 3.5
  endif()
endif()

function(get_real_target_name output target)
  get_target_property("${output}" "${target}" ALIASED_TARGET)
  if("${output}" STREQUAL "")
    set("${output}" "${target}")
  endif()
  set("${output}"
      ${${output}}
      PARENT_SCOPE)
endfunction()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)
# depends on GNUInstallDirs
configure_file(configure.h.in ${CMAKE_CURRENT_BINARY_DIR}/configure.h @ONLY)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

# build ll-build-utils only
if(BUILD_LINGLONG_BUILDER_UTILS_IN_BOX)
  pfl_init(AUTO)
  pfl_add_libraries(
    LIBS
    api
    ocppi
    oci-cfg-generators
    APPS
    ll-builder-utils
    uab)
  return()
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# NOTE(black_desk): Qt keywords conflict with glib.
add_definitions("-DQT_NO_KEYWORDS")

# NOTE(black_desk): Enable Qt logging with context.
add_definitions("-DQT_MESSAGELOGCONTEXT")

# FIXME: can not start container since the kernel does not support the
# CLONE_NEWUSER feature in the chroot environment, reference:
# https://man7.org/linux/man-pages/man2/unshare.2.html. so we skip font cache
# generator by LINGLONG_FONT_CACHE_GENERATOR, it can be removed when the above
# problem is solved.
set(ENABLE_FONT_CACHE_GENERATOR
    OFF
    CACHE BOOL "enable font cache generator")

if(ENABLE_FONT_CACHE_GENERATOR)
  add_definitions(-DLINGLONG_FONT_CACHE_GENERATOR)
endif()

# we need support Qt 5 versions older than 5.15
# https://doc.qt.io/qt-6/cmake-qt5-and-qt6-compatibility.html#supporting-qt-5-versions-older-than-5-15
if(${QT_VERSION_MAJOR})
  message(STATUS "trying specified Qt version: ${QT_VERSION_MAJOR}")
  find_package(
    Qt${QT_VERSION_MAJOR}
    COMPONENTS Core DBus
    REQUIRED)
else()
  message(STATUS "resolving Qt version automatically")

  while(true)
    find_package(Qt6 COMPONENTS Core DBus)
    if(Qt6_FOUND)
      set(QT_VERSION_MAJOR "6")
      break()
    endif()

    find_package(Qt5 COMPONENTS Core DBus)
    if(Qt5_FOUND)
      set(QT_VERSION_MAJOR "5")
      break()
    endif()

    message(FATAL_ERROR "Qt not found")
  endwhile()
endif()

message(STATUS "compiling with Qt${QT_VERSION_MAJOR}")

pkg_search_module(glib2 REQUIRED IMPORTED_TARGET glib-2.0)
pkg_search_module(ostree1 REQUIRED IMPORTED_TARGET ostree-1)
pkg_search_module(systemd REQUIRED IMPORTED_TARGET libsystemd)
pkg_search_module(ELF REQUIRED IMPORTED_TARGET libelf)

set(ytj_ENABLE_TESTING NO)
set(ytj_ENABLE_INSTALL NO)

pfl_init(AUTO)

add_subdirectory(external/http)

pfl_add_libraries(
  LIBS
  api
  dbus-api
  utils
  ocppi
  linglong
  oci-cfg-generators
  APPS
  ll-init
  ll-dialog
  ll-builder
  ll-cli
  ll-package-manager
  llpkg
  ll-session-helper)

if(ENABLE_LINGLONG_APP_BUILDER_UTILS)
  add_subdirectory(apps/ll-builder-utils)
endif()

add_subdirectory(misc)
add_subdirectory(po)
