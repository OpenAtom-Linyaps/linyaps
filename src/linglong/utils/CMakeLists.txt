# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

add_subdirectory(serialize)

add_library(linglong_utils INTERFACE)
target_link_libraries(linglong_utils INTERFACE linglong_utils_serialize)
