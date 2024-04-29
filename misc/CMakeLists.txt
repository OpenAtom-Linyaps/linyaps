# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

include(GNUInstallDirs)

function(configure_files)
  foreach(input ${ARGV})
    configure_file(${input} ${input} @ONLY)
  endforeach()
endfunction()

configure_files(
  # find -regex '\.\/\(share\|lib\|libexec\)\/.*' -type f -printf '%P\n' | sort
  libexec/linglong/create-linglong-dirs
  libexec/linglong/app-conf-generator
  libexec/linglong/fetch-dsc-repo
  libexec/linglong/fetch-git-repo
  libexec/linglong/upgrade-all
  libexec/linglong/builder/helper/config-check.sh
  libexec/linglong/builder/helper/ldd-check.sh
  libexec/linglong/builder/helper/main-check.sh
  lib/linglong/container/config.d/00-id-mapping
  lib/linglong/container/config.d/05-initialize
  lib/linglong/container/config.d/10-basics.json
  lib/linglong/container/config.d/20-devices
  lib/linglong/container/config.d/25-host-rootfs.json
  lib/linglong/container/config.d/25-host-statics.json
  lib/linglong/container/config.d/30-user-home
  lib/linglong/container/config.d/40-host-ipc
  lib/linglong/container/config.d/90-legacy
  lib/linglong/container/config.json
  lib/linglong/container/README.md
  lib/systemd/system-environment-generators/61-linglong
  lib/systemd/system-preset/91-linglong.preset
  lib/systemd/system/org.deepin.linglong.PackageManager.service
  lib/systemd/user-environment-generators/61-linglong
  lib/systemd/user/linglong-upgrade.service
  lib/systemd/user/linglong-upgrade.timer
  lib/sysusers.d/linglong.conf
  share/bash-completion/completions/ll-builder
  share/bash-completion/completions/ll-cli
  share/dbus-1/system.d/org.deepin.linglong.PackageManager.conf
  share/dbus-1/system-services/org.deepin.linglong.PackageManager.service
  share/linglong/builder/config-check.sh
  share/linglong/builder/ldd-check.sh
  share/linglong/builder/main-check.sh
  share/linglong/builder/templates/appimage-local.yaml
  share/linglong/builder/templates/appimage-url.yaml
  share/linglong/builder/templates/example.yaml
  share/mime/packages/deepin-linglong.xml
  script/linglong.sh)

# bash-completion
set(BASH_COMPLETIONS_DIR ${CMAKE_INSTALL_DATADIR}/bash-completion)

install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/share/bash-completion/completions
        DESTINATION ${BASH_COMPLETIONS_DIR})

# dbus
set(DBUS_SYSTEM_BUS_DIR ${CMAKE_INSTALL_DATADIR}/dbus-1)

install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/share/dbus-1/system.d
        DESTINATION ${DBUS_SYSTEM_BUS_DIR})
install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/share/dbus-1/system-services
        DESTINATION ${DBUS_SYSTEM_BUS_DIR})

# linglong

install(
  DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib/linglong/
  DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/linglong
  USE_SOURCE_PERMISSIONS)

# linglong default configuration
install(FILES ${CMAKE_CURRENT_LIST_DIR}/share/linglong/config.yaml
        DESTINATION ${CMAKE_INSTALL_PREFIX}/share/linglong)

# builder templates
install(DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/share/linglong/builder/templates/
        DESTINATION ${CMAKE_INSTALL_PREFIX}/share/linglong/builder/templates)

# libexec

install(
  DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/libexec/linglong/
  DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/linglong
  USE_SOURCE_PERMISSIONS)

# systemd
set(SYSTEMD_SYSTEM_PRESET_PATH ${CMAKE_INSTALL_PREFIX}/lib/systemd/system-preset)
install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib/systemd/system-preset/
        DESTINATION ${SYSTEMD_SYSTEM_PRESET_PATH})

set(SYSTEMD_SYSTEM_UNIT_PATH ${CMAKE_INSTALL_PREFIX}/lib/systemd)
install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib/systemd/system
        DESTINATION ${SYSTEMD_SYSTEM_UNIT_PATH})

set(SYSTEMD_USER_UNIT_PATH ${CMAKE_INSTALL_PREFIX}/lib/systemd)
install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib/systemd/user
        DESTINATION ${SYSTEMD_USER_UNIT_PATH})

set(SYSTEMD_USER_GENERATOR_PATH
    ${CMAKE_INSTALL_PREFIX}/lib/systemd/user-environment-generators)
install(
  DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib/systemd/user-environment-generators/
  DESTINATION ${SYSTEMD_USER_GENERATOR_PATH})

set(SYSTEMD_SYSTEM_GENERATOR_PATH
    ${CMAKE_INSTALL_PREFIX}/lib/systemd/system-environment-generators)
install(
  DIRECTORY
    ${CMAKE_CURRENT_BINARY_DIR}/lib/systemd/system-environment-generators/
  DESTINATION ${SYSTEMD_SYSTEM_GENERATOR_PATH})

# sysuser

set(SYSTEMD_SYSUSERSDIR ${CMAKE_INSTALL_PREFIX}/lib/sysusers.d)
install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib/sysusers.d/
        DESTINATION ${SYSTEMD_SYSUSERSDIR})

# xdg
install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/share/mime/packages
        DESTINATION ${CMAKE_INSTALL_DATADIR}/mime)

# set linglong XDG_DATA_DIRS environtment
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/script/linglong.sh
        DESTINATION /etc/profile.d
)

install(
  FILES ${CMAKE_CURRENT_BINARY_DIR}/script/linglong.sh
  DESTINATION /etc/X11/Xsession.d/
  RENAME 21linglong
)

install(
        FILES ${CMAKE_SOURCE_DIR}/api/schema/v1.json
        DESTINATION ${CMAKE_INSTALL_PREFIX}/share/linglong/api/
        RENAME api.json
)
