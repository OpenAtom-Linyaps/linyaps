add_subdirectory(module)

add_subdirectory(package_manager)

add_subdirectory(service)

add_subdirectory(system_helper)

add_subdirectory(builder)

add_executable(ll-builder ll-builder.main.cpp)
target_link_libraries(ll-builder PRIVATE builder)
install(TARGETS ll-builder RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

add_executable(ll-cli ll-cli.main.cpp)
target_link_libraries(
  ll-cli
  PRIVATE linglong_lib
          package-manager
          lib_ll-service
)
install(TARGETS ll-cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
# https://stackoverflow.com/a/42697475 and https://stackoverflow.com/a/74888297
install(
  CODE "execute_process(
      COMMAND ${CMAKE_COMMAND} -E create_symlink ll-cli llpkg
      WORKING_DIRECTORY \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}
    )"
)

add_executable(ll-package-manager ./ll-package-manager.main.cpp)
target_link_libraries(ll-package-manager PRIVATE package-manager)
install(TARGETS ll-package-manager RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
