# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

linglong_add_library(
  version
  SOURCES
  semver.cpp
  version.cpp
  HEADERS
  semver.h
  version.h
  DEPENDENCIES
  Qt::Core
)
