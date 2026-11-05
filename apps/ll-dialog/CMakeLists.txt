# SPDX-FileCopyrightText: 2024 - 2026 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

pfl_add_executable(
  OUTPUT_NAME
  ll-dialog
  LIBEXEC
  linglong
  SOURCES
  ./src/main.cpp
  ./src/permission_dialog_result.h
  ./src/permissionDialog.h
  ./src/permissionDialog.cpp
  LINK_LIBRARIES
  PRIVATE
  Qt${QT_VERSION_MAJOR}::Widgets
  linglong::utils
  linglong::api)
