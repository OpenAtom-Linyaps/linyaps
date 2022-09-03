set(LL_SYSTEM_HELPER_SOURCES
    # cmake-format: sortable
    ../module/util/json.h
    ../module/util/serialize/serialize.h
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
