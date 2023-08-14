# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

linglong_add_library(
  qserializer
  SOURCES
  dbus.cpp
  deprecated.cpp
  json.cpp
  qserializer.cpp
  yaml.cpp
  HEADERS
  dbus.h
  deprecated.h
  json.h
  qserializer.h
  yaml.h
  DEPENDENCIES
  Qt::DBus
  yaml-cpp
)
