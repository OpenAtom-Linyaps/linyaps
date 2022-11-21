set(LL_SYSTEM_HELPER_SOURCES
    # cmake-format: sortable
    ../module/package/info.cpp
    ../module/package/info.h
    ../module/package/ref.h
    ../module/runtime/app_config.h
    ../module/runtime/oci.h
    ../module/util/file.cpp
    ../module/util/file.h
    ../module/util/log/log_handler.cpp
    ../module/util/serialize/json.h
    ../module/util/serialize/serialize.h
    ../module/util/sysinfo.cpp
    main.cpp
    privilege/privilege_install_portal.cpp
    privilege/privilege_install_portal.h
    system_helper.cpp
    system_helper.h
)

qt5_add_dbus_adaptor(
  LL_SYSTEM_HELPER_SOURCES
  ../module/dbus_ipc/org.deepin.linglong.SystemHelper.xml
  system_helper.h
  linglong::system::helper::SystemHelper
)

set(LINK_LIBS
    yaml-cpp
    Qt5::Core
    Qt5::DBus
)

add_executable(ll-system-helper ${LL_SYSTEM_HELPER_SOURCES})

target_link_libraries(ll-system-helper PRIVATE ${LINK_LIBS})

install(TARGETS ll-system-helper RUNTIME DESTINATION bin)

install(FILES misc/org.deepin.linglong.SystemHelper.conf
        DESTINATION ${CMAKE_INSTALL_PREFIX}/share/dbus-1/system.d
)

install(FILES misc/org.deepin.linglong.SystemHelper.service
        DESTINATION ${CMAKE_INSTALL_PREFIX}/share/dbus-1/system-services
)
install(FILES misc/systemd/org.deepin.linglong.SystemHelper.service
        DESTINATION /lib/systemd/system
)
