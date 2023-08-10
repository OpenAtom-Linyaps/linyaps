set(LINGLONG_FILESET_BASEDIRS ${PROJECT_SOURCE_DIR}/src
                              ${PROJECT_BINARY_DIR}/src
)

add_subdirectory(linglong)

add_subdirectory(dbusgen)

add_subdirectory(module)

add_subdirectory(package_manager)

add_subdirectory(service)

add_subdirectory(system_helper)

add_subdirectory(builder)
