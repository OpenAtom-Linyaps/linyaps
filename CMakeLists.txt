# SPDX-FileCopyrightText: 2023 - 2026 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

cmake_minimum_required(VERSION 3.11.4)

project(
  linglong
  VERSION 1.12.0
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

option(ENABLE_CPM "enable CPM module" ON)

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

set(LINGLONG_ENABLE_WAYLAND_SEC_CTX_SUPPORT
    OFF
    CACHE BOOL "enable linglong wayland security context support")

# harden the build
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_EXE_LINKER_FLAGS
    "${CMAKE_EXE_LINKER_FLAGS} -pie -Wl,-z,relro -Wl,-z,now")

add_compile_options(-fstack-protector-strong)
if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
  message(STATUS "Non-Debug build, enabling FORTIFY_SOURCE")
  add_compile_definitions(_FORTIFY_SOURCE=2)
endif()

string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" SYSTEM_PROC_LOWER)
if(SYSTEM_PROC_LOWER MATCHES "^mips")
  message(STATUS "MIPS architecture detected. Adding -mxgot to C/CXX flags.")
  add_compile_options(-mxgot)
endif()

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(PFL)

if(CMAKE_VERSION VERSION_LESS "3.14")
  set(ENABLE_CPM NO)
  message(
    STATUS "cmake version ${CMAKE_VERSION} not compatible with CPM.cmake.")
endif()

# we don't support CPM_LOCAL_PACKAGES_ONLY
if(CPM_LOCAL_PACKAGES_ONLY)
  set(ENABLE_CPM NO)
endif()

if(ENABLE_CPM)
  include(CPM)
endif()

if(BUILD_LINGLONG_BUILDER_UTILS_IN_BOX)
  set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
endif()

# Use CPMFindPackage if ENABLE_CPM is enabled.
# Otherwise, use find_package, find_library or
# include the header-only version from external directory
function(gen_library_target name)
  set(is_static ${BUILD_LINGLONG_BUILDER_UTILS_IN_BOX})

  if("${name}" STREQUAL "tl-expected")
    set(output "tl::expected")
    if(ENABLE_CPM)
      CPMFindPackage(
        NAME tl-expected
        VERSION
          1.2.0 # NOTE: Upstream cmake version doesn't match git tag, we need
                # https://github.com/TartanLlama/expected/pull/62 in v1.1.0, but
                # cmake version of v1.1.0 is 1.0.0. upstream fix this issue in
                # https://github.com/TartanLlama/expected/commit/17d0058a5a5d3e0ea647884f9b29c518dd06f26e
                # so we require tl-expected >= 1.2.0
        GITHUB_REPOSITORY TartanLlama/expected
        GIT_TAG v1.3.1
        GIT_SHALLOW ON
        OPTIONS "EXPECTED_BUILD_TESTS OFF"
        EXCLUDE_FROM_ALL ON)
    else()
      find_package(tl-expected 1.2.0 QUIET)
      if(NOT tl-expected_FOUND)
        message(STATUS "tl-expected not found, using external tl-expected")
        add_library(tl-expected INTERFACE)
        add_library(${output} ALIAS tl-expected)
        target_include_directories(tl-expected
                                   INTERFACE ./external/tl-expected/include)
        # somebody calls find_package with tl-expected later should works
        list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake.external)
        set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} PARENT_SCOPE)
      endif()
    endif()
  elseif("${name}" STREQUAL "nlohmann_json")
    set(output "nlohmann_json::nlohmann_json")
    if(ENABLE_CPM)
      CPMFindPackage(
        NAME nlohmann_json
        VERSION 3.5.0
        URL "https://github.com/nlohmann/json/archive/refs/tags/v3.5.0.tar.gz"
        EXCLUDE_FROM_ALL ON
        OPTIONS "JSON_BuildTests OFF")
    else()
      find_package(nlohmann_json 3.5.0 QUIET)
      if(NOT nlohmann_json_FOUND)
        list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake.fix)
        set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} PARENT_SCOPE)
        find_package(nlohmann_json 3.5.0 REQUIRED)
      endif()
    endif()
  elseif("${name}" STREQUAL "yaml-cpp")
    set(output "yaml-cpp::yaml-cpp")
    if(ENABLE_CPM)
      CPMFindPackage(
        NAME yaml-cpp
        VERSION 0.6.2
        GITHUB_REPOSITORY jbeder/yaml-cpp
        GIT_TAG 0.8.0 # NOTE: When use this project with CPM.cmake(FetchContent), we
                      # need https://github.com/jbeder/yaml-cpp/pull/583 in v0.8.0
        EXCLUDE_FROM_ALL ON
        OPTIONS "YAML_CPP_INSTALL ON" "YAML_CPP_BUILD_TESTS OFF")
    else()
      if(is_static)
        message(STATUS "use static yaml-cpp")
        find_library(yaml-cpp yaml-cpp)
        add_library(yaml-cpp INTERFACE)
        target_link_libraries(yaml-cpp INTERFACE ${yaml-cpp})
      else()
        find_package(yaml-cpp REQUIRED)
      endif()
    endif()
    if(NOT TARGET ${output})
      add_library(${output} ALIAS yaml-cpp)
    endif()
  elseif("${name}" STREQUAL "CLI11")
    set(output "CLI11::CLI11")
    if(ENABLE_CPM)
      CPMFindPackage(
        NAME CLI11
        VERSION 2.4.1
        GITHUB_REPOSITORY CLIUtils/CLI11
        GIT_TAG v2.6.1
        EXCLUDE_FROM_ALL ON
        OPTIONS "CLI11_BUILD_TESTS OFF")
    else()
      find_package(CLI11 2.4.1 QUIET)
      if(NOT CLI11_FOUND)
        message(STATUS "CLI11 not found, using external CLI11")
        add_library(CLI11 INTERFACE)
        add_library(${output} ALIAS CLI11)
        target_include_directories(CLI11 INTERFACE ./external/CLI11)
      endif()
    endif()
  elseif("${name}" STREQUAL "gtest" OR "${name}" STREQUAL "gmock")
    if("${name}" STREQUAL "gtest")
      set(output "GTest::gtest")
    elseif("${name}" STREQUAL "gmock")
      set(output "GTest::gmock")
    endif()
    if(ENABLE_CPM)
      CPMFindPackage(
        NAME googletest
        GITHUB_REPOSITORY google/googletest
        GIT_TAG v1.14.0
        VERSION 1.12.1
        OPTIONS "INSTALL_GTEST OFF" "gtest_force_shared_crt"
        FIND_PACKAGE_ARGUMENTS "NAMES GTest"
        GIT_SHALLOW ON
        EXCLUDE_FROM_ALL ON)
    else()
      find_package(GTest REQUIRED)
    endif()
  elseif("${name}" STREQUAL "fmt")
    set(output "fmt::fmt-lib")
    find_package(fmt 5.2.1 QUIET)
    if(NOT fmt_FOUND)
      message(STATUS "system fmt not found, using external fmt")
      add_library(fmt-lib INTERFACE)
      add_library(${output} ALIAS fmt-lib)
      target_include_directories(fmt-lib INTERFACE external/fmt/include)
      target_compile_definitions(fmt-lib INTERFACE FMT_HEADER_ONLY)
    else()
      if(is_static)
        message(STATUS "use fmt header only")
        add_library(${output} ALIAS fmt::fmt-header-only)
      else()
        add_library(${output} ALIAS fmt::fmt)
      endif()
    endif()
  endif()

  if(NOT TARGET ${output})
    message(FATAL_ERROR "library ${name} not found target ${output}")
  else()
    message(STATUS "library ${name} found: ${output}")
  endif()
endfunction()

find_package(PkgConfig REQUIRED)

gen_library_target(tl-expected)
gen_library_target(nlohmann_json)
gen_library_target(yaml-cpp)
gen_library_target(CLI11)
gen_library_target(fmt)
if(ENABLE_TESTING)
  include(CTest)
  pkg_search_module(CRYPTO REQUIRED IMPORTED_TARGET libcrypto)
  gen_library_target(gtest)
  gen_library_target(gmock)
endif()

set(linglong_EXTERNALS "ytj ytj::ytj")

if(LINGLONG_ENABLE_WAYLAND_SEC_CTX_SUPPORT)
  message(STATUS "enable linglong wayland security context support")
  pkg_search_module(WAYLAND_CLIENT REQUIRED IMPORTED_TARGET wayland-client)
  pkg_search_module(WAYLAND_PROTOCOLS REQUIRED IMPORTED_TARGET
                    wayland-protocols)
  find_program(WAYLAND_SCANNER wayland-scanner REQUIRED)
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

pkg_search_module(cap REQUIRED IMPORTED_TARGET libcap)
pkg_search_module(systemd REQUIRED IMPORTED_TARGET libsystemd)

# build ll-build-utils only
if(BUILD_LINGLONG_BUILDER_UTILS_IN_BOX)
  pfl_init(AUTO)
  pfl_add_libraries(
    LIBS
    api
    common
    oci-cfg-generators
    cdi
    ocppi
    utils
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
    if(Qt6_FOUND AND Qt6_VERSION VERSION_GREATER_EQUAL "6.10")
      find_package(Qt6 COMPONENTS DBusPrivate)
    endif()
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
pkg_search_module(ELF REQUIRED IMPORTED_TARGET libelf)
pkg_search_module(uuid REQUIRED IMPORTED_TARGET uuid)

set(ytj_ENABLE_TESTING NO)
set(ytj_ENABLE_INSTALL NO)

pfl_init(AUTO)

add_subdirectory(external/http)

pfl_add_libraries(
  LIBS
  api
  common
  dbus-api
  utils
  ocppi
  linglong
  oci-cfg-generators
  cdi
  APPS
  ll-init
  ll-dialog
  ll-builder
  ll-cli
  ll-package-manager
  llpkg
  ll-session-helper
  ll-driver-detect)

if(ENABLE_LINGLONG_APP_BUILDER_UTILS)
  add_subdirectory(apps/ll-builder-utils)
endif()

add_subdirectory(misc)
add_subdirectory(po)
