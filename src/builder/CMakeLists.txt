file(
  GLOB_RECURSE
  LL_BUILDER_SOURCES
  FOLLOW_SYMLINKS
  CONFIGURE_DEPENDS
  *.cpp
  *.h
  *.qrc
)

add_executable(ll-builder ${LL_BUILDER_SOURCES})

target_include_directories(
  ll-builder
  PRIVATE ../
          ${GLIB_INCLUDE_DIRS}
          ${OSTREE1_INCLUDE_DIRS}
)

target_link_libraries(ll-builder PRIVATE linglong_lib)

install(TARGETS ll-builder RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
