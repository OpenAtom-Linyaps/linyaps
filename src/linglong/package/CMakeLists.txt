# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

linglong_add_library(
  package
  SOURCES
  bundle.cpp
  info.cpp
  package.cpp
  ref.cpp
  HEADERS
  bundle.h
  info.h
  package.h
  ref.h
  DEPENDENCIES
  Qt::Core
  linglong_util_qserializer
)
