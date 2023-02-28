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

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

include(GNUInstallDirs)

include_directories(${linglong_SOURCE_DIR}/src)
# NOTE(black_desk): for dbus adaptor remove later.
set(CMAKE_INCLUDE_CURRENT_DIR ON)

add_subdirectory(src)
add_subdirectory(test)
add_subdirectory(misc)
