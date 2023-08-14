# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

linglong_add_library(
  oci
  SOURCES
  distribution_client.cpp
  HEADERS
  distribution_client.h
  DEPENDENCIES
  linglong_util_qserializer
  linglong_util_http
)
