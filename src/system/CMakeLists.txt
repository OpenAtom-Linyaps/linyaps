find_package(CURL REQUIRED)
find_package(PkgConfig REQUIRED)
# QT sqlite 支持
find_package(
  Qt5
  COMPONENTS Concurrent Sql
  REQUIRED
)
pkg_search_module(OSTREE1 REQUIRED ostree-1)

set(LL_SYSTEM_SERVICE_SOURCES
    # cmake-format: sortable
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
    ../module/runtime/container.cpp
    ../module/runtime/container.cpp
    ../module/runtime/oci.h
    ../module/util/config/config.h
    ../module/util/connection.cpp
    ../module/util/desktop_entry.cpp
    ../module/util/file.cpp
    ../module/util/http/http_client.cpp
    ../module/util/http/http_client.h
    ../module/util/httpclient.cpp
    ../module/util/json.h
    ../module/util/log_handler.cpp
    ../module/util/sysinfo.cpp
    ../module/util/uuid.cpp
    ../module/util/xdg.cpp
    ../service/impl/param_option.h
    impl/app_status.cpp
    impl/appinfo_cache.cpp
    impl/job.cpp
    impl/job_manager.cpp
    impl/package_manager_flatpak_impl.cpp
    impl/system_package_manager.cpp
    main.cpp
)

qt5_add_dbus_adaptor(
  LL_SYSTEM_SERVICE_SOURCES
  ../module/dbus_ipc/com.deepin.linglong.SystemPackageManager.xml
  impl/system_package_manager.h linglong::service::SystemPackageManager
)

qt5_add_dbus_adaptor(
  LL_SYSTEM_SERVICE_SOURCES
  ../module/dbus_ipc/com.deepin.linglong.JobManager.xml impl/job_manager.h
  JobManager
)

qt5_add_dbus_adaptor(
  LL_SYSTEM_SERVICE_SOURCES ../module/dbus_ipc/com.deepin.linglong.Job.xml
  impl/job.h Job
)

message(STATUS "current CPU ARCH is: ${CMAKE_HOST_SYSTEM_PROCESSOR}")

set(LINK_LIBS ${LINK_LIBS} CURL::libcurl ${OSTREE1_LDFLAGS} Qt5::Sql)

add_executable(ll-system-helper ${LL_SYSTEM_SERVICE_SOURCES})

target_include_directories(ll-system-helper PRIVATE ${OSTREE1_INCLUDE_DIRS})

target_link_libraries(ll-system-helper PRIVATE ${LINK_LIBS})

install(TARGETS ll-system-helper RUNTIME DESTINATION bin)

install(FILES data/com.deepin.linglong.SystemPackageManager.conf
        DESTINATION ${CMAKE_INSTALL_PREFIX}/share/dbus-1/system.d
)

install(FILES data/deepin-linglong-SystemHelper.service
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/systemd/system
)

install(FILES data/deepin-linglong-SystemPackageManager.service
        DESTINATION ${CMAKE_INSTALL_PREFIX}/share/dbus-1/system-services
)
