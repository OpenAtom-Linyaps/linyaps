add_executable(ll-installer
               installer-dialog.cpp
               main.cpp)

find_package(Qt5Widgets REQUIRED)

target_link_libraries(ll-installer PRIVATE ${LINK_LIBS} Qt5::Widgets)

#install(TARGETS ll-installer RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(TARGETS ll-installer RUNTIME DESTINATION bin)

install(FILES space.linglong.Installer.desktop DESTINATION ${CMAKE_INSTALL_PREFIX}/share/applications)