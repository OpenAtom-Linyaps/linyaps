# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

# workaround for private header
include_directories(${linglong_SOURCE_DIR}/src)
include_directories(${linglong_BINARY_DIR}/src)

function(
  linglong_add_dbus_adaptor
  target_name
  xml
  header
  parent_class
  basename
)

  set(DBUS_ADAPTOR_SOURCE)
  qt5_add_dbus_adaptor(
    DBUS_ADAPTOR_SOURCE
    ${xml}
    ${header}
    ${parent_class}
    ${basename}
  )
  add_library(${target_name} OBJECT)
  target_sources(${target_name} PRIVATE ${DBUS_ADAPTOR_SOURCE})
  target_link_libraries(${target_name} PUBLIC Qt::DBus)
  target_include_directories(${target_name} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
  if(ARGN)
    target_link_libraries(${target_name} PRIVATE ${ARGN})
  endif()
endfunction()

linglong_add_dbus_adaptor(
  linglong_PackageManager_adaptor
  ${PROJECT_SOURCE_DIR}/api/dbus/org.deepin.linglong.PackageManager.xml
  linglong/package_manager/package_manager.h
  linglong::service::PackageManager
  PackageManagerAdaptor
)

linglong_add_dbus_adaptor(
  linglong_JobManager_adaptor
  ${PROJECT_SOURCE_DIR}/api/dbus/org.deepin.linglong.JobManager.xml
  linglong/job_manager/job_manager.h
  JobManager
  JobManagerAdaptor
)

linglong_add_dbus_adaptor(
  linglong_Job_adaptor
  ${PROJECT_SOURCE_DIR}/api/dbus/org.deepin.linglong.Job.xml
  linglong/job_manager/job.h
  Job
  JobAdaptor
)

linglong_add_dbus_adaptor(
  linglong_AppManager_adaptor
  ${PROJECT_SOURCE_DIR}/api/dbus/org.deepin.linglong.AppManager.xml
  linglong/service/app_manager.h
  linglong::service::AppManager
  AppManagerAdaptor
)

linglong_add_dbus_adaptor(
  linglong_PackageManagerHelper_adaptor
  ${PROJECT_SOURCE_DIR}/api/dbus/org.deepin.linglong.PackageManagerHelper.xml
  linglong/system_helper/package_manager_helper.h
  linglong::system::helper::PackageManagerHelper
  PackageManagerHelperAdaptor
)

linglong_add_dbus_adaptor(
  linglong_FilesystemHelper_adaptor
  ${PROJECT_SOURCE_DIR}/api/dbus/org.deepin.linglong.FilesystemHelper.xml
  linglong/system_helper/filesystem_helper.h
  linglong::system::helper::FilesystemHelper
  FilesystemHelperAdaptor
)

