set(LL_BUILDER_SOURCES
    ../module/package/bundle.cpp
    ../module/package/info.cpp
    ../module/package/package.cpp
    ../module/repo/ostree.cpp
    ../module/runtime/oci.h
    ../module/runtime/app.cpp
    ../module/runtime/container.cpp
    ../module/flatpak/flatpak_manager.cpp
    ../module/util/file.cpp
    ../module/util/http_client.cpp
    ../module/util/http_client.h
    ../module/util/httpclient.cpp
    ../module/util/json.h
    ../module/util/sysinfo.cpp
    ../module/util/uuid.cpp
    ../module/util/xdg.cpp
    ../module/util/desktop_entry.cpp
    ../module/util/log_handler.cpp
    builder/builder.cpp
    builder/builder_config.cpp
    builder/builder_config.h
    builder/project.cpp
    builder/project.h
    builder/source_fetcher.cpp
    builder/source_fetcher.h
    resource/resource.qrc
    builder/depend_fetcher.cpp
    builder/depend_fetcher.h)

set (RESOURCES
    resource/template.yaml
    resource/default.yaml
    resource/autotools.yaml
    resource/qmake.yaml
    resource/cmake.yaml)

find_package(CURL REQUIRED)

aux_source_directory(. LL_BUILDER_SOURCES)
aux_source_directory(builder LL_BUILDER_SOURCES)

add_executable(ll-builder ${LL_BUILDER_SOURCES})

target_include_directories(ll-builder PRIVATE ../)

target_link_libraries(ll-builder PRIVATE ${LINK_LIBS} CURL::libcurl)

install(TARGETS ll-builder RUNTIME DESTINATION bin)
install(FILES ${RESOURCES} DESTINATION share/deepin/ll-builder/template)
