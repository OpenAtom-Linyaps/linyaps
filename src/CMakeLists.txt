set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

add_definitions("-DQT_MESSAGELOGCONTEXT")
add_definitions("-DQT_NO_KEYWORDS")

find_package(PkgConfig REQUIRED)
find_package(Qt5Core REQUIRED)
find_package(Qt5DBus REQUIRED)
find_package(Qt5Network REQUIRED)
find_package(Qt5Sql REQUIRED)
find_package(yaml-cpp REQUIRED)
pkg_search_module(
  GLIB
  REQUIRED
  glib-2.0
)

set(Qt_LIBS
    Qt5::Core
    Qt5::DBus
    Qt5::Network
    Qt5::Sql
)

set(LINK_LIBS
    ${Qt_LIBS}
    yaml-cpp
    stdc++
)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)

configure_file(configure.h.in configure.h)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_subdirectory(service)

add_subdirectory(package_manager)

add_subdirectory(system_helper)

add_subdirectory(cli)

add_subdirectory(builder)
