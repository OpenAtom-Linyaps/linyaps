set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)

configure_file(configure.h.in configure.h)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_subdirectory(service)

add_subdirectory(package_manager)

add_subdirectory(system_helper)

add_subdirectory(cli)

add_subdirectory(builder)
