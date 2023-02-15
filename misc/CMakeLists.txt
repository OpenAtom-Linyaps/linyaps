# bash-completion

set(BASH_COMPLETIONS bash_completion/ll-cli)

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

configure_file(
  dbus/org.deepin.linglong.PackageManager.conf.in
  dbus/org.deepin.linglong.PackageManager.conf
)
configure_file(
  dbus/org.deepin.linglong.SystemHelper.conf.in
  dbus/org.deepin.linglong.SystemHelper.conf
)

set(DBUS_CONFIG_FILE
    # cmake-format: sortable
    ${CMAKE_CURRENT_BINARY_DIR}/dbus/org.deepin.linglong.PackageManager.conf
    ${CMAKE_CURRENT_BINARY_DIR}/dbus/org.deepin.linglong.SystemHelper.conf
)

configure_file(
  dbus/org.deepin.linglong.PackageManager.service.in
  dbus/org.deepin.linglong.PackageManager.service
  @ONLY
)
configure_file(
  dbus/org.deepin.linglong.SystemHelper.service.in
  dbus/org.deepin.linglong.SystemHelper.service
  @ONLY
)
configure_file(
  dbus/org.deepin.linglong.AppManager.service.in
  dbus/org.deepin.linglong.AppManager.service
  @ONLY
)

set(DBUS_SYSTEM_SERVICE_FILE
    # cmake-format: sortable
    ${CMAKE_CURRENT_BINARY_DIR}/dbus/org.deepin.linglong.PackageManager.service
    ${CMAKE_CURRENT_BINARY_DIR}/dbus/org.deepin.linglong.SystemHelper.service
)

set(DBUS_SESSION_SERVICE_FILE
    ${CMAKE_CURRENT_BINARY_DIR}/dbus/org.deepin.linglong.AppManager.service
)

# dbus will not load files in /usr/local/share/dbus-1/system.d. So we have to
# make sure the .conf file(s) installed to /usr/share
install(FILES ${DBUS_CONFIG_FILE} DESTINATION /usr/share/dbus-1/system.d)

install(FILES ${DBUS_SYSTEM_SERVICE_FILE}
        DESTINATION ${CMAKE_INSTALL_DATADIR}/dbus-1/system-services
)

install(FILES ${DBUS_SESSION_SERVICE_FILE}
        DESTINATION ${CMAKE_INSTALL_DATADIR}/dbus-1/services
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

# scripts

configure_file(
  scripts/create-linglong-dirs.in
  scripts/create-linglong-dirs
  @ONLY
)

set(LINGLONG_SCRIPTS ${CMAKE_CURRENT_BINARY_DIR}/scripts/create-linglong-dirs)

install(PROGRAMS ${LINGLONG_SCRIPTS}
        DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/linglong/
)

# systemd

configure_file(
  systemd/service/org.deepin.linglong.AppManager.service.in
  systemd/service/org.deepin.linglong.AppManager.service
  @ONLY
)
configure_file(
  systemd/service/org.deepin.linglong.PackageManager.service.in
  systemd/service/org.deepin.linglong.PackageManager.service
  @ONLY
)
configure_file(
  systemd/service/org.deepin.linglong.SystemHelper.service.in
  systemd/service/org.deepin.linglong.SystemHelper.service
  @ONLY
)

set(SYSTEMD_SYSTEM_SERVICE_FILE
    # cmake-format: sortable
    ${CMAKE_CURRENT_BINARY_DIR}/systemd/service/org.deepin.linglong.PackageManager.service
    ${CMAKE_CURRENT_BINARY_DIR}/systemd/service/org.deepin.linglong.SystemHelper.service
)

install(FILES ${SYSTEMD_SYSTEM_SERVICE_FILE}
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/systemd/system/
)

set(SYSTEMD_USER_SERVICE_FILE
    ${CMAKE_CURRENT_BINARY_DIR}/systemd/service/org.deepin.linglong.AppManager.service
)

install(FILES ${SYSTEMD_USER_SERVICE_FILE}
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/systemd/user/
)

configure_file(
  systemd/user-environment-generators/61-linglong.in
  systemd/user-environment-generators/61-linglong
  @ONLY
)
configure_file(
  systemd/system-environment-generators/61-linglong.in
  systemd/system-environment-generators/61-linglong
  @ONLY
)

set(SYSTEMD_USER_ENV_GEN
    ${CMAKE_CURRENT_BINARY_DIR}/systemd/user-environment-generators/61-linglong
)
set(SYSTEMD_SYSTEM_ENV_GEN
    ${CMAKE_CURRENT_BINARY_DIR}/systemd/system-environment-generators/61-linglong
)

install(
  PROGRAMS ${SYSTEMD_USER_ENV_GEN}
  DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/systemd/user-environment-generators/
)
install(
  PROGRAMS ${SYSTEMD_SYSTEM_ENV_GEN}
  DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/systemd/system-environment-generators/
)

# sysuser

pkg_get_variable(
  SYSUSERSDIR
  systemd
  sysusersdir
)

configure_file(
  sysuser/linglong.conf.in
  sysuser/linglong.conf
  @ONLY
)

set(SYSUSER_CONF_FILE ${CMAKE_CURRENT_BINARY_DIR}/sysuser/linglong.conf)

install(FILES ${SYSUSER_CONF_FILE} DESTINATION ${SYSUSERSDIR})

# xdg

install(FILES xdg/mime/deepin-linglong.xml
        DESTINATION ${CMAKE_INSTALL_DATADIR}/mime/packages
)
