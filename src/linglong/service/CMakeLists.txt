# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

linglong_add_library(
  service
  SOURCES
  app_manager.cpp
  HEADERS
  app_manager.h
  app_manager_p.h
  DEPENDENCIES
  linglong_dbusipc
  linglong_repo
  linglong_AppManager_adaptor
)
