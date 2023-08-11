add_library(
  linglong_api_v1_dbus_interfaces
  ./app_manager.cpp
  ./filesystem_helper.cpp
  ./package_manager_helper.cpp
)

linglong_private_library_add_headers(
  linglong_api_v1_dbus_interfaces
  ./app_manager.h
  ./filesystem_helper.h
  ./package_manager_helper.h
)

target_link_libraries(
  linglong_api_v1_dbus_interfaces PUBLIC linglong_dbusipc linglong_api_v1_types
)
target_include_directories(
  linglong_api_v1_dbus_interfaces PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
)

function(
  linglong_api_v1_dbus_interfaces_FN_add_interface
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
  target_sources(linglong_api_v1_dbus_interfaces PRIVATE ${INTERFACE_SOURCES})
endfunction()

linglong_api_v1_dbus_interfaces_fn_add_interface(
  ${PROJECT_SOURCE_DIR}/api/dbus/org.deepin.linglong.PackageManagerHelper.xml
  gen_org_deepin_linglong_packagemanagerhelper
)

linglong_api_v1_dbus_interfaces_fn_add_interface(
  ${PROJECT_SOURCE_DIR}/api/dbus/org.deepin.linglong.FilesystemHelper.xml
  gen_org_deepin_linglong_filesystemhelper
)

set_source_files_properties(
  ${PROJECT_SOURCE_DIR}/api/dbus/org.deepin.linglong.AppManager.xml
  PROPERTIES INCLUDE linglong/dbus_ipc/workaround.h
)

linglong_api_v1_dbus_interfaces_fn_add_interface(
  ${PROJECT_SOURCE_DIR}/api/dbus/org.deepin.linglong.AppManager.xml
  gen_org_deepin_linglong_appmanager
)

set_source_files_properties(
  ${PROJECT_SOURCE_DIR}/api/dbus/org.deepin.linglong.PackageManager.xml
  PROPERTIES INCLUDE linglong/dbus_ipc/workaround.h
)

linglong_api_v1_dbus_interfaces_fn_add_interface(
  ${PROJECT_SOURCE_DIR}/api/dbus/org.deepin.linglong.PackageManager.xml
  gen_org_deepin_linglong_packagemanager
)
