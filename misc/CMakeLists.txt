# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

configure_file(
  systemd/user/linglong-upgrade.service.in
  systemd/user/linglong-upgrade.service
  @ONLY
)

set(SYSTEMD_USER_FILE
    ${CMAKE_CURRENT_BINARY_DIR}/systemd/user/linglong-upgrade.service
    systemd/user/linglong-upgrade.timer
)

install(FILES ${SYSTEMD_USER_FILE}
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/systemd/user/
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
