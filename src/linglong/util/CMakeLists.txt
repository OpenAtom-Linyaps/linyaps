# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

configure_file(configure.h.in configure.h)

linglong_add_library(
  util
  SOURCES
  app_status.cpp
  appinfo_cache.cpp
  command_helper.cpp
  configure.cpp
  connection.cpp
  desktop_entry.cpp
  env.cpp
  erofs.cpp
  error.cpp
  file.cpp
  runner.cpp
  singleton.cpp
  status_code.cpp
  sysinfo.cpp
  uuid.cpp
  xdg.cpp
  HEADERS
  app_status.h
  appinfo_cache.h
  command_helper.h
  ${CMAKE_CURRENT_BINARY_DIR}/configure.h
  connection.h
  desktop_entry.h
  env.h
  erofs.h
  error.h
  file.h
  runner.h
  singleton.h
  status_code.h
  sysinfo.h
  uuid.h
  xdg.h
  SUBDIRS
  config
  http
  oci
  qserializer
  test
  version
  DEPENDENCIES
  linglong_api
  Qt::Core
  Qt::Sql
)
