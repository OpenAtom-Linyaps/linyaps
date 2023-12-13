# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

include(GNUInstallDirs)

# bash-completion

set(BASH_COMPLETIONS bash_completion/ll-cli)

install(FILES ${BASH_COMPLETIONS}
        DESTINATION ${CMAKE_INSTALL_DATADIR}/bash-completion/completions)

# dbus

configure_file(dbus/org.deepin.linglong.PackageManager.conf.in
               dbus/org.deepin.linglong.PackageManager.conf)
configure_file(dbus/org.deepin.linglong.SystemHelper.conf.in
               dbus/org.deepin.linglong.SystemHelper.conf)

set(DBUS_CONFIG_FILE
    # cmake-format: sortable
    ${CMAKE_CURRENT_BINARY_DIR}/dbus/org.deepin.linglong.PackageManager.conf
    ${CMAKE_CURRENT_BINARY_DIR}/dbus/org.deepin.linglong.SystemHelper.conf)

configure_file(dbus/org.deepin.linglong.PackageManager.service.in
               dbus/org.deepin.linglong.PackageManager.service @ONLY)
configure_file(dbus/org.deepin.linglong.SystemHelper.service.in
               dbus/org.deepin.linglong.SystemHelper.service @ONLY)
configure_file(dbus/org.deepin.linglong.AppManager.service.in
               dbus/org.deepin.linglong.AppManager.service @ONLY)

set(DBUS_SYSTEM_SERVICE_FILE
    # cmake-format: sortable
    ${CMAKE_CURRENT_BINARY_DIR}/dbus/org.deepin.linglong.PackageManager.service
    ${CMAKE_CURRENT_BINARY_DIR}/dbus/org.deepin.linglong.SystemHelper.service)

set(DBUS_SESSION_SERVICE_FILE
    ${CMAKE_CURRENT_BINARY_DIR}/dbus/org.deepin.linglong.AppManager.service)

# dbus will not load files in /usr/local/share/dbus-1/system.d. So we have to
# make sure the .conf file(s) installed to /usr/share
install(FILES ${DBUS_CONFIG_FILE} DESTINATION /usr/share/dbus-1/system.d)

install(FILES ${DBUS_SYSTEM_SERVICE_FILE}
        DESTINATION ${CMAKE_INSTALL_DATADIR}/dbus-1/system-services)

install(FILES ${DBUS_SESSION_SERVICE_FILE}
        DESTINATION ${CMAKE_INSTALL_DATADIR}/dbus-1/services)

# linglong

set(LINGLONG_CONFIG_FILES linglong/config.yaml)

install(FILES ${LINGLONG_CONFIG_FILES}
        DESTINATION ${CMAKE_INSTALL_DATADIR}/linglong)

set(LINGLONG_PERMISSION_FILES
    # cmake-format: sortable
    linglong/permission/calendar.json
    linglong/permission/camera.json
    linglong/permission/hostdir.json
    linglong/permission/mic.json
    linglong/permission/screenshot.json
    linglong/permission/userdir.json)

install(FILES ${LINGLONG_PERMISSION_FILES}
        DESTINATION ${CMAKE_INSTALL_DATADIR}/permission/policy/linglong)

set(LINGLONG_BUILDER_CONFIG_FILES linglong/builder/config.yaml)

install(FILES ${LINGLONG_BUILDER_CONFIG_FILES}
        DESTINATION ${CMAKE_INSTALL_DATADIR}/linglong/builder)

set(BUILDER_TEMPLATE_FILES
    # cmake-format: sortable
    linglong/builder/templates/autotools.yaml
    linglong/builder/templates/cmake.yaml
    linglong/builder/templates/default.yaml
    linglong/builder/templates/example.yaml
    linglong/builder/templates/qmake.yaml
    linglong/builder/templates/appimage.yaml
    linglong/builder/templates/makeimage.yaml)

install(FILES ${BUILDER_TEMPLATE_FILES}
        DESTINATION ${CMAKE_INSTALL_DATADIR}/linglong/builder/templates)

# scripts

configure_file(scripts/create-linglong-dirs.in scripts/create-linglong-dirs
               @ONLY)

configure_file(scripts/upgrade-all.in scripts/upgrade-all @ONLY)

set(LINGLONG_SCRIPTS ${CMAKE_CURRENT_BINARY_DIR}/scripts/create-linglong-dirs
                     ${CMAKE_CURRENT_BINARY_DIR}/scripts/upgrade-all)

install(PROGRAMS ${LINGLONG_SCRIPTS}
        DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/linglong/)

# systemd

configure_file(systemd/user/org.deepin.linglong.AppManager.service.in
               systemd/user/org.deepin.linglong.AppManager.service @ONLY)
configure_file(systemd/system/org.deepin.linglong.PackageManager.service.in
               systemd/system/org.deepin.linglong.PackageManager.service @ONLY)
configure_file(systemd/system/org.deepin.linglong.SystemHelper.service.in
               systemd/system/org.deepin.linglong.SystemHelper.service @ONLY)
configure_file(systemd/user/linglong-upgrade.service.in
               systemd/user/linglong-upgrade.service @ONLY)

set(SYSTEMD_SYSTEM_FILE
    # cmake-format: sortable
    ${CMAKE_CURRENT_BINARY_DIR}/systemd/system/org.deepin.linglong.PackageManager.service
    ${CMAKE_CURRENT_BINARY_DIR}/systemd/system/org.deepin.linglong.SystemHelper.service
)

install(FILES ${SYSTEMD_SYSTEM_FILE}
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/systemd/system/)

set(SYSTEMD_USER_FILE
    ${CMAKE_CURRENT_BINARY_DIR}/systemd/user/org.deepin.linglong.AppManager.service
    ${CMAKE_CURRENT_BINARY_DIR}/systemd/user/linglong-upgrade.service
    systemd/user/linglong-upgrade.timer)

install(FILES ${SYSTEMD_USER_FILE}
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/systemd/user/)

configure_file(systemd/user-environment-generators/61-linglong.in
               systemd/user-environment-generators/61-linglong @ONLY)
configure_file(systemd/system-environment-generators/61-linglong.in
               systemd/system-environment-generators/61-linglong @ONLY)

set(SYSTEMD_USER_ENV_GEN
    ${CMAKE_CURRENT_BINARY_DIR}/systemd/user-environment-generators/61-linglong)
set(SYSTEMD_SYSTEM_ENV_GEN
    ${CMAKE_CURRENT_BINARY_DIR}/systemd/system-environment-generators/61-linglong
)

install(
  PROGRAMS ${SYSTEMD_USER_ENV_GEN}
  DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/systemd/user-environment-generators/)
install(
  PROGRAMS ${SYSTEMD_SYSTEM_ENV_GEN}
  DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/systemd/system-environment-generators/
)

# sysuser

pkg_get_variable(SYSUSERSDIR systemd sysusersdir)

configure_file(sysuser/linglong.conf.in sysuser/linglong.conf @ONLY)

set(SYSUSER_CONF_FILE ${CMAKE_CURRENT_BINARY_DIR}/sysuser/linglong.conf)

install(FILES ${SYSUSER_CONF_FILE} DESTINATION ${SYSUSERSDIR})

# xdg

install(FILES xdg/mime/deepin-linglong.xml
        DESTINATION ${CMAKE_INSTALL_DATADIR}/mime/packages)
