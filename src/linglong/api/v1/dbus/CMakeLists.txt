linglong_add_library(
  dbus
  SOURCES
  app_manager1.cpp
  filesystem_helper.cpp
  package_manager_helper.cpp
  HEADERS
  app_manager1.h
  filesystem_helper.h
  package_manager_helper.h
  DEPENDENCIES
  linglong_dbusipc
  linglong_api_v1_types
)

target_include_directories(
  ${LINGLONG_LIB_PREFIX}dbus PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
)

function(
  linglong_add_dbus_interface
  xml
  basename
)
  set_source_files_properties(
    ${xml}
    NO_NAMESPACE
    ON
  )
  set(INTERFACE_SOURCES)
  qt5_add_dbus_interface(
    INTERFACE_SOURCES
    ${xml}
    ${basename}
  )
  target_sources(${LINGLONG_LIB_PREFIX}dbus PRIVATE ${INTERFACE_SOURCES})
endfunction()

linglong_add_dbus_interface(
  ${PROJECT_SOURCE_DIR}/api/dbus/org.deepin.linglong.PackageManagerHelper.xml
  gen_org_deepin_linglong_packagemanagerhelper
)

linglong_add_dbus_interface(
  ${PROJECT_SOURCE_DIR}/api/dbus/org.deepin.linglong.FilesystemHelper.xml
  gen_org_deepin_linglong_filesystemhelper
)

set_source_files_properties(
  ${PROJECT_SOURCE_DIR}/api/dbus/org.deepin.linglong.AppManager1.xml
  PROPERTIES INCLUDE linglong/dbus_ipc/workaround.h
)

linglong_add_dbus_interface(
  ${PROJECT_SOURCE_DIR}/api/dbus/org.deepin.linglong.AppManager1.xml
  gen_org_deepin_linglong_appmanager1
)

set_source_files_properties(
  ${PROJECT_SOURCE_DIR}/api/dbus/org.deepin.linglong.PackageManager.xml
  PROPERTIES INCLUDE linglong/dbus_ipc/workaround.h
)

linglong_add_dbus_interface(
  ${PROJECT_SOURCE_DIR}/api/dbus/org.deepin.linglong.PackageManager.xml
  gen_org_deepin_linglong_packagemanager
)
