# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

linglong_add_library(
  types
  SOURCES
  CommonOptions.cpp
  CommonReply.cpp
  DBusProxyOptions.cpp
  ExecuteOptions.cpp
  StartOptions.cpp
  HEADERS
  CommonOptions.h
  CommonReply.h
  DBusProxyOptions.h
  ExecuteOptions.h
  StartOptions.h
  DEPENDENCIES
  QSerializer::DBus
)
