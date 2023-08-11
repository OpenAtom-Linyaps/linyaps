# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

add_library(linglong_systemhelper_privilege privilege_install_portal.cpp)
linglong_private_library_add_headers(
  linglong_systemhelper_privilege privilege_install_portal.h
)
target_link_libraries(
  linglong_systemhelper_privilege PUBLIC linglong_util linglong_package
)
