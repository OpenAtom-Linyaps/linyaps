set(LL_SERVICE_SOURCES
    main.cpp
    resource/resource.qrc
    impl/package_manager.cpp
    impl/package_manager_impl.cpp
    impl/version.h
    impl/uap_manager.cpp
    impl/job_manager.cpp
    impl/job_manager.h
    impl/job.cpp
    impl/job.h
    impl/json_register_inc.h
    impl/qdbus_retmsg.h
    impl/dbus_retcode.h
    ../module/package/pkginfo.h
    ../module/package/info.cpp
    ../module/repo/repo.cpp
    ../module/runtime/container.h
    ../module/util/json.h
    ../module/util/uuid.cpp
    ../module/util/xdg.cpp
    ../module/util/desktop_entry.cpp
    ../module/runtime/oci.h
    ../module/uab/uap.cpp
    ../module/uab/uap.h
    ../module/runtime/app.cpp
    ../module/util/fs.cpp
    ../module/util/sysinfo.cpp
    ../module/util/fs.h
    ../module/runtime/tracker.h
    ../module/runtime/tracker.cpp

    # begin add by hqh debug 2021-9-16
    ../module/util/httpclient.cpp
    ../module/util/httpclient.h
    ../module/repo/repohelper.h
    ../module/repo/ostree_repohelper.cpp
    ../module/repo/repohelper_factory.cpp
    ../module/repo/ostree_repohelper.h
    # end add by hqh debug 2021-9-16
    ../module/util/runner.h
    ../module/util/singleton.h)

qt5_add_dbus_adaptor(LL_SERVICE_SOURCES
                     ../module/dbus_ipc/com.deepin.linglong.PackageManager.xml
                     impl/package_manager.h
                     PackageManager)
qt5_add_dbus_adaptor(LL_SERVICE_SOURCES
                     ../module/dbus_ipc/com.deepin.linglong.UapManager.xml
                     impl/uap_manager.h
                     UapManager)
qt5_add_dbus_adaptor(LL_SERVICE_SOURCES
                     ../module/dbus_ipc/com.deepin.linglong.JobManager.xml
                     impl/job_manager.h
                     JobManager)
qt5_add_dbus_adaptor(LL_SERVICE_SOURCES
                     ../module/dbus_ipc/com.deepin.linglong.Job.xml
                     impl/job.h
                     Job)

MESSAGE(STATUS "current CPU ARCH is: ${CMAKE_HOST_SYSTEM_PROCESSOR}")
find_package(CURL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_search_module(OSTREE1 REQUIRED ostree-1)

set(LINK_LIBS
    ${LINK_LIBS}
    CURL::libcurl
    ${OSTREE1_LDFLAGS}
    ${APPSTREMGLIB_LDFLAGS}
    -larchive
    )

add_executable(ll-service
               ${LL_SERVICE_SOURCES})

target_include_directories(ll-service PRIVATE
                           ${OSTREE1_INCLUDE_DIRS}
                           ${APPSTREMGLIB_INCLUDE_DIRS}
                           )

target_link_libraries(ll-service PRIVATE
                      ${LINK_LIBS}
                      )

install(TARGETS ll-service RUNTIME DESTINATION bin)
install(FILES data/space.linglong.Service.desktop DESTINATION ${CMAKE_INSTALL_PREFIX}/share/applications)
install(FILES data/space.linglong.Service.desktop DESTINATION /etc/xdg/autostart)
