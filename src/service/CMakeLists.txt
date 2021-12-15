find_package(CURL REQUIRED)
find_package(PkgConfig REQUIRED)
# QT sqlite支持
find_package(Qt5Sql REQUIRED)
pkg_search_module(OSTREE1 REQUIRED ostree-1)

set(LL_SERVICE_SOURCES
    main.cpp
    resource/resource.qrc
    impl/package_manager.cpp
    impl/package_manager_impl.cpp
    impl/package_manager_flatpak_impl.cpp
    impl/job_manager.cpp
    impl/job.cpp
    impl/qdbus_retmsg.h
    ../module/package/info.cpp
    ../module/repo/repo.cpp
    ../module/util/uuid.cpp
    ../module/util/xdg.cpp
    ../module/util/desktop_entry.cpp
    ../module/runtime/app.cpp
    ../module/util/fs.cpp
    ../module/util/sysinfo.cpp
    ../module/runtime/tracker.cpp
    ../module/util/httpclient.cpp
    ../module/repo/ostree_repohelper.cpp
    ../module/repo/repohelper_factory.cpp
    ../module/package/package.h
    ../module/runtime/container.h
    ../module/util/json.h
    ../module/runtime/oci.h
    ../module/util/app_status.cpp
    ../module/util/appinfo_cache.cpp)

qt5_add_dbus_adaptor(LL_SERVICE_SOURCES
                     ../module/dbus_ipc/com.deepin.linglong.PackageManager.xml
                     impl/package_manager.h
                     PackageManager)


qt5_add_dbus_adaptor(LL_SERVICE_SOURCES
                     ../module/dbus_ipc/com.deepin.linglong.JobManager.xml
                     impl/job_manager.h
                     JobManager)

qt5_add_dbus_adaptor(LL_SERVICE_SOURCES
                     ../module/dbus_ipc/com.deepin.linglong.Job.xml
                     impl/job.h
                     Job)

MESSAGE(STATUS "current CPU ARCH is: ${CMAKE_HOST_SYSTEM_PROCESSOR}")

set(LINK_LIBS
    ${LINK_LIBS}
    CURL::libcurl
    ${OSTREE1_LDFLAGS}
    Qt5::Sql)

add_executable(ll-service ${LL_SERVICE_SOURCES})

target_include_directories(ll-service
                           PRIVATE ${OSTREE1_INCLUDE_DIRS})

target_link_libraries(ll-service
                      PRIVATE ${LINK_LIBS})

install(TARGETS ll-service
        RUNTIME DESTINATION bin)

install(FILES data/config
        DESTINATION /deepin/linglong/config)

install(FILES data/space.linglong.Service.desktop
        DESTINATION ${CMAKE_INSTALL_PREFIX}/share/applications)

install(FILES data/space.linglong.Service.desktop
        DESTINATION /etc/xdg/autostart)
