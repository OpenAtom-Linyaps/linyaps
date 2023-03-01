set(LL_SYSTEM_SERVICE_SOURCES
    # cmake-format: sortable
    impl/app_status.cpp
    impl/appinfo_cache.cpp
    impl/job.cpp
    impl/job_manager.cpp
    impl/package_manager.cpp
    impl/package_manager_flatpak_impl.cpp
    main.cpp
)

add_executable(ll-package-manager ${LL_SYSTEM_SERVICE_SOURCES})

target_link_libraries(ll-package-manager PRIVATE linglong_lib)

install(TARGETS ll-package-manager RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
