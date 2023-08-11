# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

add_library(linglong_util_http http_client.cpp)
linglong_private_library_add_headers(linglong_util_http http_client.h)

target_link_libraries(linglong_util_http PUBLIC Qt5::Network)
