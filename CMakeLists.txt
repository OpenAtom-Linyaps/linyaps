# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

cmake_minimum_required(VERSION 3.21)

project(
  linglong
  VERSION 1.4.0
  DESCRIPTION "a container based package manager for deepin"
  HOMEPAGE_URL "https://github.com/linuxdeepin/linglong"
  LANGUAGES CXX
)

set(LINGLONG_USERNAME
    "deepin-linglong"
    CACHE STRING "the username for linglong package manager"
)
set(LINGLONG_ROOT
    "/var/lib/linglong"
    CACHE STRING "the location where linglong related program puts their data"
)

# build external projects
set(CMAKE_POLICY_DEFAULT_CMP0126 NEW)

set(QSERIALIZER_QT_VERSION_MAJOR "5")
add_subdirectory(external/qserializer)

# qt

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# NOTE(black_desk): qt keywords conflict with glib
add_definitions("-DQT_NO_KEYWORDS")
add_definitions("-DQT_MESSAGELOGCONTEXT")

find_package(tl-expected REQUIRED)

find_package(
  Qt5
  COMPONENTS Core
             Concurrent
             DBus
             Network
             WebSockets
             Sql
  REQUIRED
)

# other dependencies

find_package(yaml-cpp REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_search_module(
  glib2
  REQUIRED
  IMPORTED_TARGET
  glib-2.0
)
pkg_search_module(
  ostree1
  REQUIRED
  IMPORTED_TARGET
  ostree-1
)
pkg_search_module(
  systemd
  REQUIRED
  IMPORTED_TARGET
  systemd
)

include(GNUInstallDirs)

add_subdirectory(src)
add_subdirectory(apps)
add_subdirectory(misc)

include(CTest)
if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
