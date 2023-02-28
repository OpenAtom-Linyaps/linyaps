set(LL_SERVICE_SOURCES
    # cmake-format: sortable
    ../module/dbus_ipc/param_option.h
    ../module/dbus_ipc/reply.h
    ../module/flatpak/flatpak_manager.cpp
    ../module/package/info.cpp
    ../module/package/package.cpp
    ../module/repo/ostree_repo.cpp
    ../module/repo/ostree_repo.h
    ../module/repo/ostree_repohelper.cpp
    ../module/repo/repo.cpp
    ../module/repo/repo_client.cpp
    ../module/repo/repo_client.h
    ../module/runtime/app.cpp
    ../module/runtime/app_config.h
    ../module/runtime/container.cpp
    ../module/runtime/oci.h
    ../module/util/config/config.h
    ../module/util/connection.cpp
    ../module/util/desktop_entry.cpp
    ../module/util/file.cpp
    ../module/util/http/http_client.cpp
    ../module/util/http/http_client.h
    ../module/util/http/httpclient.cpp
    ../module/util/log/log_handler.cpp
    ../module/util/serialize/json.h
    ../module/util/sysinfo.cpp
    ../module/util/uuid.cpp
    ../module/util/xdg.cpp
    ../package_manager/impl/app_status.cpp
    ../package_manager/impl/appinfo_cache.cpp
    impl/app_manager.cpp
    main.cpp
    resource/resource.qrc
)

qt5_add_dbus_adaptor(
  LL_SERVICE_SOURCES
  ../module/dbus_ipc/org.deepin.linglong.AppManager.xml
  impl/app_manager.h
  linglong::service::AppManager
)

message(STATUS "current CPU ARCH is: ${CMAKE_HOST_SYSTEM_PROCESSOR}")

add_executable(ll-service ${LL_SERVICE_SOURCES})

target_link_libraries(
  ll-service
  PRIVATE CURL::libcurl
          PkgConfig::ostree1
          yaml-cpp
          Qt5::DBus
          Qt5::Sql
          Qt5::WebSockets
)

install(TARGETS ll-service RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
