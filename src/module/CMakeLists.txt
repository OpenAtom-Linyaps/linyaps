file(
  GLOB_RECURSE
  LINGLONG_LIB_SOURCES
  FOLLOW_SYMLINKS
  CONFIGURE_DEPENDS
  *.cpp
  *.h
  *.qrc
)

configure_file(util/configure.h.in util/configure.h)

add_library(linglong_lib ${LINGLONG_LIB_SOURCES})
target_link_libraries(
  linglong_lib
  PUBLIC Qt5::DBus
         Qt5::Network
         Qt5::Sql
         PkgConfig::glib2
         PkgConfig::ostree1
         yaml-cpp
         linglong_PackageManager_adaptor
         linglong_JobManager_adaptor
         linglong_Job_adaptor
         linglong_AppManager_adaptor
         linglong_PackageManagerHelper_adaptor
         linglong_FilesystemHelper_adaptor
         linglong_PackageManager_interface
         linglong_AppManager_interface
         linglong_PackageManagerHelper_interface
         linglong_FilesystemHelper_interface
)
