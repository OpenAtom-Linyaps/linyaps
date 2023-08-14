# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

linglong_add_library(
  http
  SOURCES
  http_client.cpp
  HEADERS
  http_client.h
  DEPENDENCIES
  Qt5::Network
)
