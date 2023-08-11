# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

add_library(
  linglong_dbusipc
  dbus_common.cpp
  dbus_system_helper_common.cpp
  package_manager_param.cpp
  param_option.cpp
  reply.cpp
  workaround.cpp
)

linglong_private_library_add_headers(
  linglong_dbusipc
  dbus_common.h
  dbus_system_helper_common.h
  package_manager_param.h
  param_option.h
  reply.h
  workaround.h
)

target_link_libraries(linglong_dbusipc PUBLIC Qt::DBus Qt::Core)
