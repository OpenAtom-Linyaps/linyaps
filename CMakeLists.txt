# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

cmake_minimum_required(VERSION 3.11.4)

project(
  linglong
  VERSION 1.6.1
  DESCRIPTION "a container based application package manager for Linux desktop"
  HOMEPAGE_URL "https://github.com/OpenAtom-Linyaps/linyaps"
  LANGUAGES CXX)

set(LINGLONG_VERSION
    ""
    CACHE STRING "The version of linglong project.")

if(NOT ("${LINGLONG_VERSION}" STREQUAL ""))
  message(STATUS "Project version has been overridden to ${LINGLONG_VERSION}")
  set(PROJECT_VERSION ${LINGLONG_VERSION})
endif()

set(ENABLE_UAB OFF CACHE BOOL "enable building UAB")
set(ENABLE_LINGLONG_INSTALLER OFF CACHE BOOL "enable linglong installer")

set(LINGLONG_USERNAME
    "deepin-linglong"
    CACHE STRING "The username for linglong package manager")
set(LINGLONG_ROOT
    "/var/lib/linglong"
    CACHE STRING "The location where linglong related program puts their data")
set(LINGLONG_HOST_PATH
    "/run/host"
    CACHE STRING "The location where linglong host path mapping")
set(LINGLONG_DEFAULT_OCI_RUNTIME
    "ll-box"
    CACHE STRING "The oci runtime which linglong use by default")

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
    NAME docopt
    VERSION 0.6.1 # NOTE: Upstream cmake version doesn't match git tag, we need
                  # https://github.com/docopt/docopt.cpp/pull/102 in v0.6.3, but
                  # cmake version of v0.6.3 is 0.6.1.
    GITHUB_REPOSITORY docopt/docopt.cpp
    GIT_SHALLOW ON
    GIT_TAG v0.6.3
    EXCLUDE_FROM_ALL ON)
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
endif()

set(linglong_EXTERNALS "ytj ytj::ytj")

# NOTE: UOS v20 do not have docopt packaged.
find_package(docopt 0.6.1 QUIET)
if(NOT docopt_FOUND)
  # list(APPEND linglong_EXTERNALS "docopt.cpp docopt.cpp") # we use header only
  add_library(docopt INTERFACE)
  target_compile_definitions(docopt INTERFACE DOCOPT_HEADER_ONLY)
  target_include_directories(docopt INTERFACE ./external/docopt.cpp)
  list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake.external)
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

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# NOTE(black_desk): Qt keywords conflict with glib.
add_definitions("-DQT_NO_KEYWORDS")

# NOTE(black_desk): Enable Qt logging with context.
add_definitions("-DQT_MESSAGELOGCONTEXT")

find_package(
  Qt5
  COMPONENTS Core DBus
  REQUIRED)

find_package(PkgConfig REQUIRED)

pkg_search_module(glib2 REQUIRED IMPORTED_TARGET glib-2.0)
pkg_search_module(ostree1 REQUIRED IMPORTED_TARGET ostree-1)
pkg_search_module(systemd REQUIRED IMPORTED_TARGET libsystemd)
pkg_search_module(ELF REQUIRED IMPORTED_TARGET libelf)

set(ytj_ENABLE_TESTING NO)
set(ytj_ENABLE_INSTALL NO)

pfl_init(AUTO)

add_subdirectory(external/http)
add_subdirectory(tools/qdbusxml2cpp)

include(GNUInstallDirs)

pfl_add_libraries(
  LIBS
  api
  dbus-api
  utils
  ocppi
  linglong
  APPS
  generators/00-id-mapping
  generators/05-initialize
  generators/20-devices
  generators/25-host-env
  generators/30-user-home
  generators/40-host-ipc
  generators/90-legacy
  ll-box
  ll-builder
  ll-cli
  ll-package-manager
  llpkg
  uab
  ll-session-helper)

add_subdirectory(misc)
