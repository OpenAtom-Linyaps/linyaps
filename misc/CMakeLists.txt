# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

configure_file(
  systemd/user/linglong-upgrade.service.in
  systemd/user/linglong-upgrade.service
  @ONLY
)

configure_file(
  dbus/org.deepin.linglong.AppManager.service.in
  dbus/org.deepin.linglong.AppManager.service
)

set(SYSTEMD_USER_FILE
    ${CMAKE_CURRENT_BINARY_DIR}/systemd/user/linglong-upgrade.service
    systemd/user/linglong-upgrade.timer
)

set(DBUS_SESSION_SERVICE_FILE
    ${CMAKE_CURRENT_BINARY_DIR}/dbus/org.deepin.linglong.AppManager.service
)

install(FILES ${SYSTEMD_USER_FILE}
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/systemd/user/
)

install(FILES ${DBUS_SESSION_SERVICE_FILE}
        DESTINATION ${CMAKE_INSTALL_DATADIR}/dbus-1/services
)

configure_file(
  scripts/upgrade-all.in
  scripts/upgrade-all
  @ONLY
)

set(LINGLONG_SCRIPTS ${CMAKE_CURRENT_BINARY_DIR}/scripts/upgrade-all)

install(PROGRAMS ${LINGLONG_SCRIPTS}
        DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/linglong/
)

#systemd 
configure_file(
  systemd/service/org.deepin.linglong.AppManager.service.in
  systemd/service/org.deepin.linglong.AppManager.service
)

set(SYSTEMD_USER_SERVICE_FILE
    ${CMAKE_CURRENT_BINARY_DIR}/systemd/service/org.deepin.linglong.AppManager.service
)

install(FILES ${SYSTEMD_USER_SERVICE_FILE}
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/systemd/user/
)