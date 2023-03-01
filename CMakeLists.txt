cmake_minimum_required(VERSION 3.13.4) # for UOS 105x

project(linglong)

set(LINGLONG_USERNAME
    "deepin-linglong"
    CACHE STRING "the username for linglong package manager"
)
set(LINGLONG_LOG_DIR
    "/var/log/linglong"
    CACHE STRING "the location where linglong releated program write their log"
)
set(LINGLONG_ROOT
    "/var/lib/linglong"
    CACHE STRING "the location where linglong releated program puts their data"
)

# for private header
include_directories(${linglong_SOURCE_DIR}/src)
include_directories(${linglong_BINARY_DIR}/src)

# NOTE(black_desk): for dbus adaptor remove later.
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# qt

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

add_definitions("-DQT_MESSAGELOGCONTEXT")
add_definitions("-DQT_NO_KEYWORDS")

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
find_package(CURL REQUIRED)
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

include(GNUInstallDirs)

add_subdirectory(src)
add_subdirectory(misc)

# enable_testing()
# FIXME(black_desk): disable test for now, enable it later.
add_subdirectory(test)
