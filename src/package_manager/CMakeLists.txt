set(LL_SYSTEM_SERVICE_SOURCES
    # cmake-format: sortable
    ../module/dbus_ipc/param_option.h
    ../module/flatpak/flatpak_manager.cpp
    ../module/package/info.cpp
    ../module/package/package.cpp
    ../module/package/package.h
    ../module/package/package.h
    ../module/repo/ostree_repo.cpp
    ../module/repo/ostree_repohelper.cpp
    ../module/repo/ostree_repohelper.cpp
    ../module/repo/repo.cpp
    ../module/repo/repo_client.cpp
    ../module/repo/repo_client.h
    ../module/runtime/app.cpp
    ../module/runtime/app_config.h
    ../module/runtime/container.cpp
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
    ../module/util/version/semver.h
    ../module/util/xdg.cpp
    impl/app_status.cpp
    impl/appinfo_cache.cpp
    impl/job.cpp
    impl/job_manager.cpp
    impl/package_manager.cpp
    impl/package_manager_flatpak_impl.cpp
    main.cpp
)

add_executable(ll-package-manager ${LL_SYSTEM_SERVICE_SOURCES})

target_link_libraries(
  ll-package-manager
  PRIVATE CURL::libcurl
          PkgConfig::ostree1
          yaml-cpp
          Qt5::DBus
          Qt5::Sql
          Qt5::WebSockets
          LLDBusIPC
)

install(TARGETS ll-package-manager RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
