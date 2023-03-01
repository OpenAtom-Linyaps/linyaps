set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)

add_subdirectory(module)

add_subdirectory(package_manager)

add_subdirectory(service)

add_subdirectory(system_helper)

add_subdirectory(cli)

add_subdirectory(builder)
