set(LL_SYSTEM_HELPER_SOURCES
    # cmake-format: sortable
    main.cpp
    privilege/privilege_install_portal.cpp
    privilege/privilege_install_portal.h
    system_helper.cpp
    system_helper.h
)

add_executable(ll-system-helper ${LL_SYSTEM_HELPER_SOURCES})

target_link_libraries(ll-system-helper PRIVATE linglong_lib)

install(TARGETS ll-system-helper RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
