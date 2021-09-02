set(LL_BUILDER_SOURCES
    main.cpp 
    ../module/util/uuid.cpp
    ../module/package/package.h
    ../module/util/uuid.h)

add_executable(upkg-builder
               ${LL_BUILDER_SOURCES})

target_link_libraries(upkg-builder PRIVATE ${LINK_LIBS})

install(TARGETS upkg-builder RUNTIME DESTINATION ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})