qt5_add_dbus_adaptor(
  LINGLONG_DBUS_IPC_SOURCES
  org.deepin.linglong.PackageManager.xml
  package_manager/impl/package_manager.h
  linglong::service::PackageManager
)

qt5_add_dbus_adaptor(
  LINGLONG_DBUS_IPC_SOURCES
  org.deepin.linglong.JobManager.xml
  package_manager/impl/job_manager.h
  JobManager
)

qt5_add_dbus_adaptor(
  LINGLONG_DBUS_IPC_SOURCES
  org.deepin.linglong.Job.xml
  package_manager/impl/job.h
  Job
)

qt5_add_dbus_adaptor(
  LINGLONG_DBUS_IPC_SOURCES
  org.deepin.linglong.AppManager.xml
  service/impl/app_manager.h
  linglong::service::AppManager
)

qt5_add_dbus_adaptor(
  LINGLONG_DBUS_IPC_SOURCES
  org.deepin.linglong.SystemHelper.xml
  system_helper/system_helper.h
  linglong::system::helper::SystemHelper
)

qt5_add_dbus_interface(
  LINGLONG_DBUS_IPC_SOURCES
  org.deepin.linglong.SystemHelper.xml
  dbus_system_helper
)

set_source_files_properties(
  org.deepin.linglong.AppManager.xml PROPERTIES INCLUDE register_meta_type.h
)

qt5_add_dbus_interface(
  LINGLONG_DBUS_IPC_SOURCES
  org.deepin.linglong.AppManager.xml
  app_manager
)

set_source_files_properties(
  org.deepin.linglong.PackageManager.xml PROPERTIES INCLUDE
                                                    register_meta_type.h
)

qt5_add_dbus_interface(
  LINGLONG_DBUS_IPC_SOURCES
  org.deepin.linglong.PackageManager.xml
  package_manager
)

add_library(LLDBusIPC ${LINGLONG_DBUS_IPC_SOURCES})
target_include_directories(
  LLDBusIPC PUBLIC ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(LLDBusIPC PUBLIC Qt5::DBus)
