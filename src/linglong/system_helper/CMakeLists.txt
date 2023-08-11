# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

add_subdirectory(privilege)

add_library(
  linglong_systemhelper filesystem_helper.cpp package_manager_helper.cpp
)
linglong_private_library_add_headers(
  linglong_systemhelper
  filesystem_helper.h
  package_manager_helper.h
)
target_link_libraries(
  linglong_systemhelper
  PUBLIC linglong_dbusipc
         linglong_systemhelper_privilege
         linglong_PackageManagerHelper_adaptor
         linglong_FilesystemHelper_adaptor
)
