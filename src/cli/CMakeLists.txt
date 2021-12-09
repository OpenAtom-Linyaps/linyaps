set(LL_CLI_SOURCES
    main.cpp
    cmd/exec.cpp
    cmd/ps.cpp
    ../module/util/json.h
    ../module/runtime/oci.h
    ../module/runtime/container.h
    ../module/package/package.h
    ../service/impl/qdbus_retmsg.h
    #add for cli no dbus support
    ../service/impl/package_manager.cpp
    ../service/impl/package_manager_impl.cpp
    ../service/impl/package_manager_flatpak_impl.cpp
    ../service/impl/job_manager.cpp
    ../service/impl/job.cpp
    ../service/resource/resource.qrc
    ../module/util/httpclient.cpp
    ../module/repo/ostree_repohelper.cpp
    ../module/repo/repohelper_factory.cpp
    ../module/util/app_status.cpp
    ../module/util/appinfo_cache.cpp
    ../module/package/info.cpp
    ../module/repo/repo.cpp
    ../module/runtime/app.cpp
    ../module/util/fs.cpp
    ../module/util/sysinfo.cpp
    ../module/util/uuid.cpp
    ../module/util/xdg.cpp
    ../module/util/desktop_entry.cpp)

set_source_files_properties(../module/dbus_ipc/com.deepin.linglong.PackageManager.xml
                            PROPERTIES INCLUDE service/impl/json_register_inc.h)

qt5_add_dbus_interface(LL_CLI_SOURCES
                       ../module/dbus_ipc/com.deepin.linglong.PackageManager.xml
                       package_manager)

#add for cli no dbus support
qt5_add_dbus_adaptor(LL_CLI_SOURCES
                     ../module/dbus_ipc/com.deepin.linglong.JobManager.xml
                     ../service/impl/job_manager.h
                     JobManager)

qt5_add_dbus_adaptor(LL_CLI_SOURCES
                     ../module/dbus_ipc/com.deepin.linglong.Job.xml
                     ../service/impl/job.h
                     Job)

find_package(CURL REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(Qt5Sql REQUIRED)
pkg_search_module(OSTREE1 REQUIRED ostree-1)
#add for cli no dbus 
set(CLI_NO_DBUS
    CURL::libcurl
    ${OSTREE1_LDFLAGS}
    Qt5::Sql)

add_executable(ll-cli ${LL_CLI_SOURCES})
#add for cli no dbus support
target_link_libraries(ll-cli PRIVATE ${LINK_LIBS} ${CLI_NO_DBUS})
target_include_directories(ll-cli PRIVATE ${OSTREE1_INCLUDE_DIRS})

install(TARGETS ll-cli RUNTIME DESTINATION bin)

install(FILES ${CMAKE_SOURCE_DIR}/scripts/linglong.sh DESTINATION /etc/profile.d)
install(FILES ${CMAKE_SOURCE_DIR}/scripts/linglong.sh DESTINATION /etc/X11/Xsession.d/ RENAME 21linglong)