# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

linglong_add_library(
  serialize
  SOURCES
  yaml.cpp
  HEADERS
  json.h
  yaml.h
  DEPENDENCIES
  QSerializer::Core
)
