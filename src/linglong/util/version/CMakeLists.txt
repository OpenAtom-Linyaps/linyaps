# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

add_library(linglong_util_version semver.cpp version.cpp)
linglong_private_library_add_headers(
  linglong_util_version
  semver.h
  version.h
)

target_link_libraries(linglong_util_version PUBLIC Qt::Core)
