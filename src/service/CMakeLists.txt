set(LL_SERVICE_SOURCES
    # cmake-format: sortable
    ../package_manager/impl/app_status.cpp
    ../package_manager/impl/appinfo_cache.cpp
    impl/app_manager.cpp
    main.cpp
)

add_executable(ll-service ${LL_SERVICE_SOURCES})

target_link_libraries(ll-service PRIVATE linglong_lib)

install(TARGETS ll-service RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
