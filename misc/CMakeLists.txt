# bash-completion

set(BASH_COMPLETIONS bash_completion/ll-cli bash_completion/llpkg)

install(FILES ${BASH_COMPLETIONS}
        DESTINATION ${CMAKE_INSTALL_DATADIR}/bash-completion/completions
)

# builder-template

set(BUILDER_TEMPLATE_FILES
    # cmake-format: sortable
    builder-template/autotools.yaml
    builder-template/cmake.yaml
    builder-template/default.yaml
    builder-template/example.yaml
    builder-template/qmake.yaml
)

install(FILES ${BUILDER_TEMPLATE_FILES}
        DESTINATION ${CMAKE_INSTALL_DATADIR}/deepin/ll-builder/template
)

# dbus

set(DBUS_CONFIG_FILE
    # cmake-format: sortable
    dbus/org.deepin.linglong.PackageManager.conf
    dbus/org.deepin.linglong.SystemHelper.conf
)

set(DBUS_SYSTEM_SERVICE_FILE
    # cmake-format: sortable
    dbus/org.deepin.linglong.PackageManager.service
    dbus/org.deepin.linglong.SystemHelper.service
)

# dbus will not load files in /usr/local/share/dbus-1/system.d. So we have to
# make sure the .conf file(s) installed to /usr/share
install(FILES ${DBUS_CONFIG_FILE} DESTINATION /usr/share/dbus-1/system.d)

install(FILES ${DBUS_SYSTEM_SERVICE_FILE}
        DESTINATION ${CMAKE_INSTALL_DATADIR}/dbus-1/system-services
)

# linglong

set(LINGLONG_CONFIG_FILES linglong/builder.yaml linglong/config.json)

install(FILES linglong/builder.yaml
        DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/linglong
)

install(FILES linglong/config.json
        DESTINATION ${CMAKE_INSTALL_DATADIR}/linglong
)

set(LINGLONG_PERMISSION_FILES
    # cmake-format: sortable
    linglong/permission/calendar.json
    linglong/permission/camera.json
    linglong/permission/hostdir.json
    linglong/permission/mic.json
    linglong/permission/screenshot.json
    linglong/permission/userdir.json
)

install(FILES ${LINGLONG_PERMISSION_FILES}
        DESTINATION ${CMAKE_INSTALL_DATADIR}/permission/policy/linglong
)

# systemd

set(SYSTEMD_SERVICE_FILE
    # cmake-format: sortable
    systemd/org.deepin.linglong.PackageManager.service
    systemd/org.deepin.linglong.SystemHelper.service
)

install(FILES ${SYSTEMD_SERVICE_FILE}
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/systemd/system/
)

# sysuser

pkg_get_variable(
  SYSUSERSDIR
  systemd
  sysusersdir
)

install(FILES sysuser/linglong.conf DESTINATION ${SYSUSERSDIR})

# xdg

install(FILES xdg/mime/deepin-linglong.xml
        DESTINATION ${CMAKE_INSTALL_DATADIR}/mime/packages
)

install(FILES xdg/desktop/org.deepin.linglong.service.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
)

install(FILES xdg/desktop/org.deepin.linglong.service.desktop
        DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/xdg/autostart
)
