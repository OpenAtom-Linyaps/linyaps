
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
add_definitions("-DQT_MESSAGELOGCONTEXT")

find_package(PkgConfig REQUIRED)
find_package(Qt5Core REQUIRED)
find_package(Qt5DBus REQUIRED)
find_package(Qt5Network REQUIRED)
find_package(Qt5Sql REQUIRED)
find_package(yaml-cpp REQUIRED)

set(Qt_LIBS Qt5::Core Qt5::DBus Qt5::Network Qt5::Sql)
set(LINK_LIBS
    ${Qt_LIBS}
    yaml-cpp
    stdc++)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)

add_subdirectory(service)

add_subdirectory(system)

add_subdirectory(cli)

add_subdirectory(builder)
