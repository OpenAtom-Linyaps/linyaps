# SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
#
# SPDX-License-Identifier: LGPL-3.0-or-later

linglong_add_library(
  appmanager
  SOURCES
  app_manager1.cpp
  HEADERS
  app_manager1.h
  DEPENDENCIES
  linglong_service
)

linglong_add_dbus_adaptor(
  linglong_adaptors_appmanager
  ${PROJECT_SOURCE_DIR}/api/dbus/org.deepin.linglong.AppManager1.xml
  linglong/service/app_manager.h
  linglong::service::AppManager
  gen_org_deepin_linglong_appmanager1
  OrgDeepinLinglongAppmanager1Adaptor
)
