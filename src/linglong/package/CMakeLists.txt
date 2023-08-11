# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

add_library(
  linglong_package
  bundle.cpp
  info.cpp
  package.cpp
  ref.cpp
)

linglong_private_library_add_headers(
  linglong_package
  bundle.h
  info.h
  package.h
  ref.h
)

target_link_libraries(
  linglong_package PUBLIC Qt::Core linglong_util_qserializer
)
