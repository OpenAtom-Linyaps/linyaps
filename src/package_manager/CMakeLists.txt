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

add_library(lib_ll-package-manager)
aux_source_directory(impl LIB_LL_PACKAGE_MANAGER_SOURCES)
target_sources(lib_ll-package-manager PRIVATE ${LIB_LL_PACKAGE_MANAGER_SOURCES})
target_link_libraries(lib_ll-package-manager PUBLIC linglong_lib)

add_executable(ll-package-manager)
aux_source_directory(. LL_PACKAGE_MANAGER_SOURCES)
target_sources(ll-package-manager PRIVATE ${LL_PACKAGE_MANAGER_SOURCES})
target_link_libraries(ll-package-manager PRIVATE lib_ll-package-manager)

install(TARGETS ll-package-manager RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
