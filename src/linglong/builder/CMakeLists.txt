# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

linglong_add_library(
  builder
  SOURCES
  builder_config.cpp
  builder_templates.qrc
  depend_fetcher.cpp
  linglong_builder.cpp
  project.cpp
  source_fetcher.cpp
  HEADERS
  builder_config.h
  builder.h
  project.h
  depend_fetcher.h
  linglong_builder.h
  source_fetcher.h
  source_fetcher_p.h
  DEPENDENCIES
  Qt::Core
  PkgConfig::glib2
  PkgConfig::ostree1
  linglong_util
)
