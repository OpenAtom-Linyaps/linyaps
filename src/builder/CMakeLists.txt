set(LL_BUILDER_SOURCES
    main.cpp)

add_executable(ll-builder
               ${LL_BUILDER_SOURCES})

target_link_libraries(ll-builder PRIVATE ${LINK_LIBS})

install(TARGETS ll-builder RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})