# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

linglong_add_library(
  packagemanager
  SOURCES
  package_manager.cpp
  HEADERS
  package_manager.h
  package_manager_p.h
  DEPENDENCIES
  linglong_runtime
  linglong_util
  linglong_jobmanager
  linglong_dbusipc
  linglong_package
  linglong_PackageManager_adaptor
)
