set(LL_CLI_SOURCES
        main.cpp
        ../module/util/json.h
        ../module/runtime/oci.h
        ../module/runtime/container.h
        cmd/exec.cpp
        cmd/ps.cpp
        cmd/cmd.h
        ../service/impl/json_register_inc.h
        ../module/package/pkginfo.h
        ../service/impl/qdbus_retmsg.h
        )

set_source_files_properties(../module/dbus_ipc/com.deepin.linglong.PackageManager.xml
        PROPERTIES INCLUDE service/impl/json_register_inc.h)

qt5_add_dbus_interface(LL_CLI_SOURCES
        ../module/dbus_ipc/com.deepin.linglong.PackageManager.xml
        package_manager)
add_executable(ll-cmd
        ${LL_CLI_SOURCES})

#target_include_directories(llc PRIVATE ${JSONSTRUCT_INCLUDE_DIR})

target_link_libraries(ll-cmd PRIVATE ${LINK_LIBS})

install(TARGETS ll-cmd RUNTIME DESTINATION bin)
