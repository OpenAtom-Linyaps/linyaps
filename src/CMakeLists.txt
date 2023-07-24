add_subdirectory(module)

add_subdirectory(package_manager)

add_subdirectory(service)

add_subdirectory(system_helper)

add_subdirectory(cli)

add_subdirectory(builder)

add_executable(ll-builder ll-builder.main.cpp)
target_link_libraries(ll-builder PRIVATE builder)
install(TARGETS ll-builder RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
