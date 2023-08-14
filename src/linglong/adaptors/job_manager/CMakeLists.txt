# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

linglong_add_library(
  jobmanager
  SOURCES
  job1.cpp
  job_manager1.cpp
  HEADERS
  job1.h
  job_manager1.h
  DEPENDENCIES
  linglong_jobmanager
)

linglong_add_dbus_adaptor(
  linglong_adaptors_jobmanager
  ${PROJECT_SOURCE_DIR}/api/dbus/org.deepin.linglong.Job1.xml
  linglong/job_manager/job.h
  linglong::job_manager::Job
  gen_org_deepin_linglong_job1
  OrgDeepinLinglongJob1Adaptor
)

linglong_add_dbus_adaptor(
  linglong_adaptors_jobmanager
  ${PROJECT_SOURCE_DIR}/api/dbus/org.deepin.linglong.JobManager1.xml
  linglong/job_manager/job_manager.h
  linglong::job_manager::JobManager
  gen_org_deepin_linglong_jobmanager1
  OrgDeepinLinglongJobmanager1Adaptor
)
