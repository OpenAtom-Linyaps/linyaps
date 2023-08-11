# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

add_library(
  linglong_packagemanager
  job.cpp
  job_manager.cpp
  package_manager.cpp
)

linglong_private_library_add_headers(
  linglong_packagemanager
  job.h
  job_manager.h
  package_manager.h
  package_manager_p.h
)

target_link_libraries(
  linglong_packagemanager
  PUBLIC linglong_runtime
         linglong_util
         linglong_repo
         linglong_dbusipc
         linglong_package
         linglong_PackageManager_adaptor
         linglong_JobManager_adaptor
         linglong_Job_adaptor
         linglong_PackageManagerHelper_adaptor
         linglong_PackageManagerHelper_interface
)
