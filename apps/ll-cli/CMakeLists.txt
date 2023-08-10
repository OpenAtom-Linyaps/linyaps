add_subdirectory(src)

install(TARGETS ll-cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
# https://stackoverflow.com/a/42697475 and https://stackoverflow.com/a/74888297
install(
  CODE "execute_process(
      COMMAND ${CMAKE_COMMAND} -E create_symlink ll-cli llpkg
      WORKING_DIRECTORY \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}
    )"
)

