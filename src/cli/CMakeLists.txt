set(LL_CLI_SOURCES
    # cmake-format: sortable
    ../module/flatpak/flatpak_manager.cpp
    ../module/package/info.cpp
    ../module/package/package.cpp
    ../module/package/package.h
    ../module/repo/ostree_repo.cpp
    ../module/repo/ostree_repohelper.cpp
    ../module/repo/repo.cpp
    ../module/repo/repo_client.cpp
    ../module/repo/repo_client.h
    ../module/runtime/app.cpp
    ../module/runtime/container.cpp
    ../module/runtime/container.h
    ../module/runtime/oci.h
    ../module/runtime/app_config.h
    ../module/util/config/config.h
    ../module/util/connection.cpp
    ../module/util/desktop_entry.cpp
    ../module/util/file.cpp
    ../module/util/http/http_client.cpp
    ../module/util/http/http_client.h
    ../module/util/http/httpclient.cpp
    ../module/util/serialize/json.h
    ../module/util/log/log_handler.cpp
    ../module/util/singleton.h
    ../module/util/sysinfo.cpp
    ../module/util/uuid.cpp
    ../module/util/xdg.cpp
    ../service/impl/app_manager.cpp
    ../module/dbus_ipc/param_option.h
    ../module/dbus_ipc/reply.h
    ../service/resource/resource.qrc
    ../package_manager/impl/app_status.cpp
    ../package_manager/impl/appinfo_cache.cpp
    ../package_manager/impl/job.cpp
    ../package_manager/impl/job_manager.cpp
    ../package_manager/impl/package_manager_flatpak_impl.cpp
    ../package_manager/impl/package_manager.cpp
    cmd/command_helper.cpp
    main.cpp
)

set_source_files_properties(
  ../module/dbus_ipc/org.deepin.linglong.AppManager.xml
  PROPERTIES INCLUDE module/dbus_ipc/register_meta_type.h
)

qt5_add_dbus_interface(
  LL_CLI_SOURCES
  ../module/dbus_ipc/org.deepin.linglong.AppManager.xml
  app_manager
)

set_source_files_properties(
  ../module/dbus_ipc/org.deepin.linglong.PackageManager.xml
  PROPERTIES INCLUDE module/dbus_ipc/register_meta_type.h
)

qt5_add_dbus_interface(
  LL_CLI_SOURCES
  ../module/dbus_ipc/org.deepin.linglong.PackageManager.xml
  package_manager
)

qt5_add_dbus_interface(
  LL_CLI_SOURCES
  ../module/dbus_ipc/org.deepin.linglong.SystemHelper.xml
  dbus_system_helper
)

# add for cli no dbus support
qt5_add_dbus_adaptor(
  LL_CLI_SOURCES
  ../module/dbus_ipc/org.deepin.linglong.JobManager.xml
  ../package_manager/impl/job_manager.h
  JobManager
)

qt5_add_dbus_adaptor(
  LL_CLI_SOURCES
  ../module/dbus_ipc/org.deepin.linglong.Job.xml
  ../package_manager/impl/job.h
  Job
)

find_package(CURL REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(Qt5 REQUIRED COMPONENTS Sql WebSockets)
pkg_search_module(
  OSTREE1
  REQUIRED
  ostree-1
)
# add for cli no dbus
set(CLI_NO_DBUS
    CURL::libcurl
    ${OSTREE1_LDFLAGS}
    Qt5::Sql
    Qt5::WebSockets
)

add_executable(ll-cli ${LL_CLI_SOURCES})
# add for cli no dbus support
target_link_libraries(ll-cli PRIVATE ${LINK_LIBS} ${CLI_NO_DBUS})
target_include_directories(ll-cli PRIVATE ${OSTREE1_INCLUDE_DIRS})

install(TARGETS ll-cli RUNTIME DESTINATION bin)

install(FILES misc/bash_completion/ll-cli
        DESTINATION ${CMAKE_INSTALL_PREFIX}/share/bash-completion/completions
)
install(FILES misc/bash_completion/llpkg
        DESTINATION ${CMAKE_INSTALL_PREFIX}/share/bash-completion/completions
)

install(FILES ${CMAKE_SOURCE_DIR}/scripts/linglong.sh
        DESTINATION /etc/profile.d
)
install(
  FILES ${CMAKE_SOURCE_DIR}/scripts/linglong.sh
  DESTINATION /etc/X11/Xsession.d/
  RENAME 21linglong
)
install(PROGRAMS ${CMAKE_SOURCE_DIR}/scripts/61-linglong
        DESTINATION /usr/lib/systemd/user-environment-generators/
)
