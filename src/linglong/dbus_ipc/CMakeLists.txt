# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

linglong_add_library(
  dbusipc
  SOURCE
  dbus_common.cpp
  dbus_system_helper_common.cpp
  package_manager_param.cpp
  param_option.cpp
  reply.cpp
  workaround.cpp
  HEADERS
  dbus_common.h
  dbus_system_helper_common.h
  package_manager_param.h
  param_option.h
  reply.h
  workaround.h
  DEPENDENCIES
  Qt::DBus
  Qt::Core
)
