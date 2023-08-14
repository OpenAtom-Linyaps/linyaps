# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

linglong_add_library(
  systemhelper
  SOURCES
  filesystem_helper.cpp
  package_manager_helper.cpp
  HEADERS
  filesystem_helper.h
  package_manager_helper.h
  SUBDIRS
  privilege
  DEPENDENCIES
  linglong_dbusipc
  linglong_systemhelper_privilege
)
