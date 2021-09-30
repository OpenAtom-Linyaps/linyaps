set(LL_BUILDER_SOURCES
    main.cpp
    ../module/util/uuid.cpp
    ../module/package/package.h
    ../module/util/uuid.h
    ../module/util/fs.h
    ../module/util/repo.h
    )

add_executable(upkg-builder
               ${LL_BUILDER_SOURCES})

       #target_include_directories(upkg-builder PRIVATE ${JSONSTRUCT_INCLUDE_DIR})

target_link_libraries(upkg-builder PRIVATE ${LINK_LIBS} -larchive -lcrypto++ )

install(TARGETS upkg-builder RUNTIME DESTINATION bin)
