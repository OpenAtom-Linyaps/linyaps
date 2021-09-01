set(LL_CLI_SOURCES
    ../module/util/json.h
    ../module/runtime/oci.h
    main.cpp)

set_source_files_properties(../module/dbus_ipc/com.deepin.linglong.PackageManager.xml
                            PROPERTIES INCLUDE module/runtime/container.h)

qt5_add_dbus_interface(LL_CLI_SOURCES
                       ../module/dbus_ipc/com.deepin.linglong.PackageManager.xml
                       package_manager)
add_executable(llc
               ${LL_CLI_SOURCES})

target_link_libraries(llc PRIVATE ${LINK_LIBS})

install(TARGETS llc RUNTIME DESTINATION ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})