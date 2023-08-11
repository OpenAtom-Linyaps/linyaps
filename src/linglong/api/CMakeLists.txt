# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

add_subdirectory(v1)

add_library(linglong_api INTERFACE)
target_link_libraries(linglong_api INTERFACE linglong_api_v1)
