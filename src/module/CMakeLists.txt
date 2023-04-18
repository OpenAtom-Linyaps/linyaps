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

# dbus adaptor

qt5_add_dbus_adaptor(
  LINGLONG_LIB_SOURCES
  dbus_ipc/org.deepin.linglong.PackageManager.xml
  package_manager/impl/package_manager.h
  linglong::service::PackageManager
  dbus_gen_package_manager_adaptor
)

qt5_add_dbus_adaptor(
  LINGLONG_LIB_SOURCES
  dbus_ipc/org.deepin.linglong.JobManager.xml
  package_manager/impl/job_manager.h
  JobManager
  dbus_gen_jop_manager_adaptor
)

qt5_add_dbus_adaptor(
  LINGLONG_LIB_SOURCES
  dbus_ipc/org.deepin.linglong.Job.xml
  package_manager/impl/job.h
  Job
  dbus_gen_jop_adaptor
)

qt5_add_dbus_adaptor(
  LINGLONG_LIB_SOURCES
  dbus_ipc/org.deepin.linglong.AppManager.xml
  service/impl/app_manager.h
  linglong::service::AppManager
  dbus_gen_app_manager_adaptor
)

qt5_add_dbus_adaptor(
  LINGLONG_LIB_SOURCES
  dbus_ipc/org.deepin.linglong.PackageManagerHelper.xml
  system_helper/package_manager_helper.h
  linglong::system::helper::PackageManagerHelper
  dbus_gen_package_manager_helper_adaptor
)

qt5_add_dbus_adaptor(
  LINGLONG_LIB_SOURCES
  dbus_ipc/org.deepin.linglong.FilesystemHelper.xml
  system_helper/filesystem_helper.h
  linglong::system::helper::FilesystemHelper
  dbus_gen_filesystem_helper_adaptor
)

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
         CURL::libcurl
)
