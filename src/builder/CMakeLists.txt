set(LL_BUILDER_SOURCES 
    ../module/util/fs.cpp
    ../module/package/bundle.cpp
    ../module/util/json.h
    resource/resource.qrc)

aux_source_directory(. LL_BUILDER_SOURCES)
aux_source_directory(builder LL_BUILDER_SOURCES)

add_executable(ll-builder ${LL_BUILDER_SOURCES})

target_include_directories(ll-builder
                           PRIVATE builder
                           PRIVATE ../module/util
                           PRIVATE ../module/package)

target_link_libraries(ll-builder
                      PRIVATE ${LINK_LIBS}
                      PRIVATE -larchive)

install(TARGETS ll-builder RUNTIME DESTINATION bin)
