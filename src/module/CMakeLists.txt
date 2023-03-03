file(
  GLOB_RECURSE
  LINGLONG_LIB_SOURCES
  FOLLOW_SYMLINKS
  CONFIGURE_DEPENDS
  *.cpp
  *.h
  *.qrc
)

qt5_add_dbus_adaptor(
  LINGLONG_LIB_SOURCES
  dbus_ipc/org.deepin.linglong.PackageManager.xml
  package_manager/impl/package_manager.h
  linglong::service::PackageManager
)

qt5_add_dbus_adaptor(
  LINGLONG_LIB_SOURCES
  dbus_ipc/org.deepin.linglong.JobManager.xml
  package_manager/impl/job_manager.h
  JobManager
)

qt5_add_dbus_adaptor(
  LINGLONG_LIB_SOURCES
  dbus_ipc/org.deepin.linglong.Job.xml
  package_manager/impl/job.h
  Job
)

qt5_add_dbus_adaptor(
  LINGLONG_LIB_SOURCES
  dbus_ipc/org.deepin.linglong.AppManager.xml
  service/impl/app_manager.h
  linglong::service::AppManager
)

qt5_add_dbus_adaptor(
  LINGLONG_LIB_SOURCES
  dbus_ipc/org.deepin.linglong.SystemHelper.xml
  system_helper/system_helper.h
  linglong::system::helper::SystemHelper
)

qt5_add_dbus_interface(
  LINGLONG_LIB_SOURCES
  dbus_ipc/org.deepin.linglong.SystemHelper.xml
  dbus_system_helper
)

set_source_files_properties(
  dbus_ipc/org.deepin.linglong.AppManager.xml
  PROPERTIES INCLUDE module/dbus_ipc/register_meta_type.h
)

qt5_add_dbus_interface(
  LINGLONG_LIB_SOURCES
  dbus_ipc/org.deepin.linglong.AppManager.xml
  app_manager
)

set_source_files_properties(
  dbus_ipc/org.deepin.linglong.PackageManager.xml
  PROPERTIES INCLUDE module/dbus_ipc/register_meta_type.h
)

qt5_add_dbus_interface(
  LINGLONG_LIB_SOURCES
  dbus_ipc/org.deepin.linglong.PackageManager.xml
  package_manager
)

add_library(linglong_lib ${LINGLONG_LIB_SOURCES})
target_include_directories(
  linglong_lib PUBLIC ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}
)
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
