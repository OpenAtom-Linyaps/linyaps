set(LL_BUILDER_SOURCES
    # cmake-format: sortable
    ../cli/cmd/command_helper.cpp
    ../module/flatpak/flatpak_manager.cpp
    ../module/package/bundle.cpp
    ../module/package/info.cpp
    ../module/package/package.cpp
    ../module/repo/ostree_repo.cpp
    ../module/repo/ostree_repo.h
    ../module/repo/ostree_repohelper.cpp
    ../module/repo/repo.cpp
    ../module/repo/repo_client.cpp
    ../module/repo/repo_client.h
    ../module/runtime/app.cpp
    ../module/runtime/container.cpp
    ../module/runtime/oci.h
    ../module/runtime/app_config.h
    ../module/util/config/config.h
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
    builder/builder.cpp
    builder/builder_config.cpp
    builder/builder_config.h
    builder/depend_fetcher.cpp
    builder/depend_fetcher.h
    builder/project.cpp
    builder/project.h
    builder/source_fetcher.cpp
    builder/source_fetcher.h
    resource/resource.qrc
)

set(RESOURCES
    # cmake-format: sortable
    resource/autotools.yaml
    resource/cmake.yaml
    resource/default.yaml
    resource/qmake.yaml
    resource/template.yaml
)

set(CONFIG_FILE
    # cmake-format: sortable
    resource/builder.yaml
)

find_package(CURL REQUIRED)

aux_source_directory(. LL_BUILDER_SOURCES)
aux_source_directory(builder LL_BUILDER_SOURCES)

add_executable(ll-builder ${LL_BUILDER_SOURCES})

target_include_directories(
  ll-builder
  PRIVATE ../
          ${GLIB_INCLUDE_DIRS}
          ${OSTREE1_INCLUDE_DIRS}
)

target_link_libraries(
  ll-builder
  PRIVATE ${LINK_LIBS}
          CURL::libcurl
          ${GLIB_LDFLAGS}
          ${OSTREE1_LDFLAGS}
)

install(TARGETS ll-builder RUNTIME DESTINATION bin)
install(FILES ${RESOURCES} DESTINATION share/deepin/ll-builder/template)
install(FILES ${CONFIG_FILE} DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/linglong)
