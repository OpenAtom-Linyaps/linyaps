set(LL_CLI_SOURCES
    ../package_manager/impl/app_status.cpp
    ../package_manager/impl/appinfo_cache.cpp
    ../package_manager/impl/job.cpp
    ../package_manager/impl/job_manager.cpp
    ../package_manager/impl/package_manager.cpp
    ../package_manager/impl/package_manager_flatpak_impl.cpp
    ../service/impl/app_manager.cpp
    ../service/resource/resource.qrc
    main.cpp
)

add_executable(ll-cli ${LL_CLI_SOURCES})

target_link_libraries(ll-cli PRIVATE linglong_lib)

install(TARGETS ll-cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# https://stackoverflow.com/a/42697475 and https://stackoverflow.com/a/74888297
install(
  CODE "execute_process(
      COMMAND ${CMAKE_COMMAND} -E create_symlink ll-cli llpkg
      WORKING_DIRECTORY \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}
    )"
)
