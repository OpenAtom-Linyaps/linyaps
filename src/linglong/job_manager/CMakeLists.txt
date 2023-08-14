# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

linglong_add_library(
  jobmanager
  SOURCES
  job.cpp
  job_manager.cpp
  HEADERS
  job.h
  job_manager.h
  DEPENDENCIES
  Qt::Core
  linglong_repo
  linglong_adaptors_jobmanager
)
