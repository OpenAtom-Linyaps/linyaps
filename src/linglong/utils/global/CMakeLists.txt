# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

linglong_add_library(
  global
  SOURCES
  initialize.cpp
  HEADERS
  initialize.h
  DEPENDENCIES
  Qt::Core
  systemd
)
