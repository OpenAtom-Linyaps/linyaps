add_library(lib_ll-service)
aux_source_directory(impl LIB_LL_SERVICE_SOURCES)
target_sources(lib_ll-service PRIVATE ${LIB_LL_SERVICE_SOURCES})
target_link_libraries(lib_ll-service PUBLIC linglong_lib lib_ll-package-manager)

add_executable(ll-service ${LL_SERVICE_SOURCES})
aux_source_directory(. LL_SERVICE_SOURCES)
target_sources(ll-service PRIVATE ${LL_SERVICE_SOURCES})
target_link_libraries(ll-service PRIVATE lib_ll-service)

install(TARGETS ll-service RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
