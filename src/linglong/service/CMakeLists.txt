# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

linglong_add_library(
  service
  SOURCES
  app_manager.cpp
  HEADERS
  app_manager.h
  DEPENDENCIES
  linglong_dbusipc
  linglong_repo
  linglong_runtime
)
