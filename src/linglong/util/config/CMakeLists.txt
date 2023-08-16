# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

linglong_add_library(
  config
  SOURCES
  config.cpp
  repo.cpp
  HEADERS
  config.h
  repo.h
  DEPENDENCIES
  Qt::Core
  linglong_util_qserializer
  linglong_repo
)
