set(LL_BUILDER_SOURCES
    main.cpp
    builder/bst_builder.cpp
    builder/project.cpp
    ../module/util/uuid.cpp
    ../module/package/package.h
    ../module/util/uuid.h
    ../module/util/fs.h
    ../module/util/result.h
    ../module/util/repo.h
    ../module/util/json.h
    )

add_executable(ll-builder
               ${LL_BUILDER_SOURCES})

target_link_libraries(ll-builder PRIVATE ${LINK_LIBS} -larchive)

install(TARGETS ll-builder RUNTIME DESTINATION bin)
