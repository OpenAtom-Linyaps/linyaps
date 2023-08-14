# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

linglong_add_library(
  packagemanager
  SOURCES
  package_manager1.cpp
  HEADERS
  package_manager1.h
  DEPENDENCIES
  linglong_packagemanager
)

linglong_add_dbus_adaptor(
  linglong_adaptors_packagemanager
  ${PROJECT_SOURCE_DIR}/api/dbus/org.deepin.linglong.PackageManager1.xml
  linglong/package_manager/package_manager.h
  linglong::service::PackageManager
  gen_org_deepin_linglong_packagemanager1
  OrgDeepinLinglongPackagemanager1Adaptor
)
