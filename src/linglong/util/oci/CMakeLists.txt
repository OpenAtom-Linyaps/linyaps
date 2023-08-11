# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

add_library(linglong_util_oci ./distribution_client.cpp)
linglong_private_library_add_headers(linglong_util_oci ./distribution_client.h)

target_link_libraries(
  linglong_util_oci
  PUBLIC linglong_util_qserializer
         linglong_util_http
)
