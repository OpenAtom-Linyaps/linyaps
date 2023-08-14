# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

linglong_add_library(
  system_helper
  SOURCES
  filesystem_helper1.cpp
  package_manager_helper1.cpp
  HEADERS
  filesystem_helper1.h
  package_manager_helper1.h
  DEPENDENCIES
  linglong_systemhelper
)

linglong_add_dbus_adaptor(
  linglong_adaptors_systemhelper
  ${PROJECT_SOURCE_DIR}/api/dbus/org.deepin.linglong.FilesystemHelper1.xml
  linglong/system_helper/filesystem_helper.h
  linglong::system::helper::FilesystemHelper # FIXME: should be
                                             # linglong::system_helper::FilesystemHelper
  gen_org_deepin_linglong_filesystemhelper1
  OrgDeepinLinglongFilesystemHelper1Adaptor
)

linglong_add_dbus_adaptor(
  linglong_adaptors_systemhelper
  ${PROJECT_SOURCE_DIR}/api/dbus/org.deepin.linglong.PackageManagerHelper1.xml
  linglong/system_helper/package_manager_helper.h
  linglong::system::helper::PackageManagerHelper # FIXME: should be
                                                 # linglong::system_helper::PackageManagerHelper
  gen_org_deepin_linglong_packagemanagerhelper1
  OrgDeepinLinglongPackageManagerHelper1Adaptor
)
