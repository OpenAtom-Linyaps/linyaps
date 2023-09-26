# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

linglong_add_library(
  dbus
  SOURCES
  register.cpp
  HEADERS
  register.h
  DEPENDENCIES
  Qt::DBus
  tl::expected
)
