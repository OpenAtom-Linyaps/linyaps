# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

add_library(
  linglong_util_qserializer
  dbus.cpp
  deprecated.cpp
  json.cpp
  qserializer.cpp
  yaml.cpp
)
linglong_private_library_add_headers(
  linglong_util_qserializer
  dbus.h
  deprecated.h
  json.h
  qserializer.h
  yaml.h
)

target_link_libraries(linglong_util_qserializer PUBLIC Qt::DBus yaml-cpp)
