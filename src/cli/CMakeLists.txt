set(LL_CLI_SOURCES
    main.cpp
    cmd/exec.cpp
    cmd/ps.cpp
    ../module/util/json.h
    ../module/runtime/oci.h
    ../module/runtime/container.h
    ../module/package/pkginfo.h
    ../service/impl/qdbus_retmsg.h)

set_source_files_properties(../module/dbus_ipc/com.deepin.linglong.PackageManager.xml
                            PROPERTIES INCLUDE service/impl/json_register_inc.h)

qt5_add_dbus_interface(LL_CLI_SOURCES
                       ../module/dbus_ipc/com.deepin.linglong.PackageManager.xml
                       package_manager)

add_executable(ll-cli ${LL_CLI_SOURCES})

target_link_libraries(ll-cli PRIVATE ${LINK_LIBS})

install(TARGETS ll-cli RUNTIME DESTINATION bin)
