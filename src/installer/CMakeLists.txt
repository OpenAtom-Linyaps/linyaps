find_package(Qt5 REQUIRED COMPONENTS Widgets)

add_executable(ll-installer
               installer-dialog.cpp
               main.cpp)

target_link_libraries(ll-installer
                      PRIVATE ${LINK_LIBS}
                      PRIVATE Qt5::Widgets)

install(TARGETS ll-installer
        RUNTIME
        DESTINATION bin)

install(FILES space.linglong.Installer.desktop
        DESTINATION ${CMAKE_INSTALL_PREFIX}/share/applications)