file(
  GLOB_RECURSE
  LL_SYSTEM_HELPER_SOURCES
  FOLLOW_SYMLINKS
  CONFIGURE_DEPENDS
  *.cpp
  *.h
)

add_library(
  linglong_lib_system_helper
  filesystem_helper.cpp
  filesystem_helper.h
  package_manager_helper.cpp
  package_manager_helper.h
  privilege/privilege_install_portal.cpp
  privilege/privilege_install_portal.h
)
add_library(linglong::system_helper ALIAS linglong_lib_system_helper)
target_link_libraries(linglong_lib_system_helper PUBLIC linglong_lib)
