# bash-completion

set(BASH_COMPLETIONS bash_completion/ll-cli bash_completion/llpkg)

install(FILES ${BASH_COMPLETIONS}
        DESTINATION ${CMAKE_INSTALL_DATADIR}/bash-completion/completions
)

# builder

set(BUILDER_TEMPLATE_FILES
    # cmake-format: sortable
    builder-template/autotools.yaml
    builder-template/cmake.yaml
    builder-template/default.yaml
    builder-template/example.yaml
    builder-template/qmake.yaml
)

set(BUILDER_CONFIG_FILE builder.yaml)

install(FILES ${BUILDER_TEMPLATE_FILES}
        DESTINATION ${CMAKE_INSTALL_DATADIR}/deepin/ll-builder/template
)
install(FILES ${BUILDER_CONFIG_FILE}
        DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/linglong
)

# dbus

set(DBUS_CONFIG_FILE dbus/org.deepin.linglong.PackageManager.conf)

set(DBUS_SERVICE_FILE dbus/org.deepin.linglong.PackageManager.service)

# dbus will not load files in /usr/local/share/dbus-1/system.d. So we have to
# make sure the .conf file(s) installed to /usr/share
install(FILES ${DBUS_CONFIG_FILE} DESTINATION /usr/share/dbus-1/system.d)

install(FILES ${DBUS_SERVICE_FILE}
        DESTINATION ${CMAKE_INSTALL_DATADIR}/dbus-1/system-services
)

# systemd

set(SYSTEMD_SERVICE_FILE systemd/org.deepin.linglong.PackageManager.service)

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
