# SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later
if (NOT ${ENABLE_LINGLONG_INSTALLER})
    return()
endif()

find_package(Qt5 REQUIRED Widgets)

pfl_add_executable(
  SOURCES
  src/main.cpp
  src/installer-dialog.cpp
  src/installer-dialog.h
  OUTPUT_NAME
  ll-installer
  LINK_LIBRARIES
  PRIVATE
  Qt5::Widgets
  linglong::linglong)

  include(GNUInstallDirs)
