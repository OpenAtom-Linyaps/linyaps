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

# dbus interface

qt5_add_dbus_interface(
  LINGLONG_LIB_SOURCES
  dbus_ipc/org.deepin.linglong.PackageManagerHelper.xml
  dbus_gen_package_manager_helper_interface
)

qt5_add_dbus_interface(
  LINGLONG_LIB_SOURCES
  dbus_ipc/org.deepin.linglong.FilesystemHelper.xml
  dbus_gen_filesystem_helper_interface
)

set_source_files_properties(
  dbus_ipc/org.deepin.linglong.AppManager.xml
  PROPERTIES INCLUDE module/dbus_ipc/workaround.h
)

qt5_add_dbus_interface(
  LINGLONG_LIB_SOURCES
  dbus_ipc/org.deepin.linglong.AppManager.xml
  dbus_gen_app_manager_interface
)

set_source_files_properties(
  dbus_ipc/org.deepin.linglong.PackageManager.xml
  PROPERTIES INCLUDE module/dbus_ipc/workaround.h
)

qt5_add_dbus_interface(
  LINGLONG_LIB_SOURCES
  dbus_ipc/org.deepin.linglong.PackageManager.xml
  dbus_gen_package_manager_interface
)

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
)
