
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
add_definitions("-DQT_MESSAGELOGCONTEXT")

find_package(PkgConfig REQUIRED)
find_package(Qt5Core REQUIRED)
find_package(Qt5DBus REQUIRED)
find_package(DtkCore REQUIRED)
find_package(yaml-cpp REQUIRED)

set(Qt_LIBS Qt5::Core Qt5::DBus)
set(DTK_LIBS dtkcore)
set(LINK_LIBS
    ${Qt_LIBS}
    ${DTK_LIBS}
    yaml-cpp
    stdc++)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)

add_subdirectory(box)

add_subdirectory(service)

add_subdirectory(cli)

add_subdirectory(builder)

add_subdirectory(installer)
