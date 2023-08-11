# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

add_subdirectory(types)

add_library(linglong_api_v1 INTERFACE)
target_link_libraries(linglong_api_v1 INTERFACE linglong_api_v1_types)
