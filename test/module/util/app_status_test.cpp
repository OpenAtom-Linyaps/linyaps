/*
 * Copyright (c) 2022. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     huqinghong <huqinghong@uniontech.com>
 *
 * Maintainer: huqinghong <huqinghong@uniontech.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include <QDebug>

#include "src/module/util/app_status.h"
#include "src/module/util/sysinfo.h"

TEST(AppStatus, GetInstalledAppInfo)
{
    linglong::util::Connection connection;
    QSqlQuery sqlQuery = connection.execute("select * from installedAppInfo");

    EXPECT_EQ(sqlQuery.lastError().type(), QSqlError::NoError);

    // 插入2条记录
    QString appDataJson =
        "{\"appId\":\"org.deepin.calculator\",\"name\":\"deepin-calculator\",\"version\":\"5.7.16\",\"arch\":\"x86_64\",\"kind\":\"app\",\"runtime\":\"org.deepin.Runtime/20.5.0/x86_64\",\"uabUrl\":\"\",\"repoName\":\"repo\",\"description\":\"Calculator for UOS\"}";

    linglong::util::checkInstalledAppDb();
    linglong::util::updateInstalledAppInfoDb();

    auto appItem = linglong::util::loadJSONString<linglong::package::AppMetaInfo>(appDataJson);
    const QString userName = linglong::util::getUserName();

    linglong::package::AppMetaInfoList pkgList;
    auto result = linglong::util::getInstalledAppInfo(appItem->appId, "", appItem->arch, "", pkgList);
    EXPECT_EQ(result, true);

    linglong::package::AppMetaInfoList pkgList1;
    result = linglong::util::getInstalledAppInfo(appItem->appId, "", appItem->arch, "", pkgList1);
    EXPECT_EQ(result, true);

    QString ret = "";
    QString error = "";
    result = linglong::util::queryAllInstalledApp("deepin-linglong", ret, error);
    EXPECT_EQ(result, true);
    linglong::package::AppMetaInfoList appMetaInfoList;
    linglong::util::getAppMetaInfoListByJson(ret, appMetaInfoList);

    auto code = linglong::util::insertAppRecord(appItem, "user", userName);
    code = linglong::util::deleteAppRecord(appItem->appId, "1.0.0", linglong::util::hostArch(), userName);
    EXPECT_NE(code, 0);
}