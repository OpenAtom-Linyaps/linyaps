# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

linglong_add_library(
  runtime
  SOURCES
  app.cpp
  app_config.cpp
  container.cpp
  oci.cpp
  resource/app_configs.qrc
  HEADERS
  app.h
  app_config.h
  container.h
  oci.h
  DEPENDENCIES
  Qt::Core
  linglong_util_qserializer
  linglong_package
)
