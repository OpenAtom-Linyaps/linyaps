# SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

pfl_add_executable(
  OUTPUT_NAME
  99-ll-dialog
  LIBEXEC
  linglong/dialog
  SOURCES
  ./src/main.cpp
  ./src/permission.h
  ./src/permission.cpp
  LINK_LIBRARIES
  PRIVATE
  Qt${QT_VERSION_MAJOR}::Widgets
  linglong::utils
  linglong::api)
