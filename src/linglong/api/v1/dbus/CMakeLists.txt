linglong_add_library(
  dbus
  SOURCES
  app_manager1.cpp
  filesystem_helper1.cpp
  package_manager_helper1.cpp
  HEADERS
  app_manager1.h
  filesystem_helper1.h
  package_manager_helper1.h
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
  ${PROJECT_SOURCE_DIR}/api/dbus/org.deepin.linglong.PackageManagerHelper1.xml
  gen_org_deepin_linglong_packagemanagerhelper1
)

linglong_add_dbus_interface(
  ${PROJECT_SOURCE_DIR}/api/dbus/org.deepin.linglong.FilesystemHelper1.xml
  gen_org_deepin_linglong_filesystemhelper1
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
  ${PROJECT_SOURCE_DIR}/api/dbus/org.deepin.linglong.PackageManager1.xml
  PROPERTIES INCLUDE linglong/dbus_ipc/workaround.h
)

linglong_add_dbus_interface(
  ${PROJECT_SOURCE_DIR}/api/dbus/org.deepin.linglong.PackageManager1.xml
  gen_org_deepin_linglong_packagemanager1
)
