set(LL_SETNS_SOURCES
    container_bash.cpp)

add_executable(ll-test-setns-srv
               ${LL_SETNS_SOURCES})

target_link_libraries(ll-test-setns-srv PRIVATE ${LINK_LIBS})


set(LL_SETNS_CLI_SOURCES
    container_bash_cli.cpp)

add_executable(ll-test-setns-cli
               ${LL_SETNS_CLI_SOURCES})

target_link_libraries(ll-test-setns-cli PRIVATE ${LINK_LIBS})