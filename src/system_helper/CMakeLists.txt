file(
  GLOB_RECURSE
  LL_SYSTEM_HELPER_SOURCES
  FOLLOW_SYMLINKS
  CONFIGURE_DEPENDS
  *.cpp
  *.h
)

add_executable(ll-system-helper)
target_sources(ll-system-helper PRIVATE ${LL_SYSTEM_HELPER_SOURCES})
target_link_libraries(ll-system-helper PRIVATE linglong_lib)

install(TARGETS ll-system-helper RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
