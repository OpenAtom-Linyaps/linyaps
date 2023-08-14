# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

linglong_add_library(
  packagemanager
  SOURCES
  job.cpp
  job_manager.cpp
  package_manager.cpp
  HEADERS
  job.h
  job_manager.h
  package_manager.h
  package_manager_p.h
  DEPENDENCIES
  linglong_runtime
  linglong_util
  linglong_repo
  linglong_dbusipc
  linglong_package
  linglong_PackageManager_adaptor
  linglong_JobManager_adaptor
  linglong_Job_adaptor
)
