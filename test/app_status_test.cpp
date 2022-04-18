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
    // 插入2条记录
    QString appDataJson =
        "{\"appId\":\"org.demo.test\",\"name\":\"test1\",\"version\":\"8.3.9\",\"arch\":\"x86_64\",\"kind\":\"app\",\"runtime\":\"org.deepin.Runtime/20.5.0/x86_64\",\"uabUrl\":\"\",\"repoName\":\"repo\",\"description\":\"demo for unit test\"}";

    auto appItem = linglong::util::loadJSONString<linglong::package::AppMetaInfo>(appDataJson);
    const QString userName = linglong::util::getUserName();
    int ret = insertAppRecord(appItem, "user", userName);
    EXPECT_EQ(ret, 0);

    QString appDataJson1 =
        "{\"appId\":\"org.demo.test\",\"name\":\"test1\",\"version\":\"8.3.21\",\"arch\":\"x86_64\",\"kind\":\"app\",\"runtime\":\"org.deepin.Runtime/20.5.0/x86_64\",\"uabUrl\":\"\",\"repoName\":\"repo\",\"description\":\"demo for unit test\"}";

    auto appItem1 = linglong::util::loadJSONString<linglong::package::AppMetaInfo>(appDataJson1);
    ret = insertAppRecord(appItem1, "user", userName);
    EXPECT_EQ(ret, 0);

    linglong::package::AppMetaInfoList pkgList;
    auto result = getInstalledAppInfo("org.demo.test", "", "x86_64", userName, pkgList);
    EXPECT_EQ(result, true);

    auto it = pkgList.at(0);
    EXPECT_EQ(it->version, "8.3.21");

    ret = deleteAppRecord("org.demo.test", "", "x86_64", userName);
    EXPECT_EQ(ret, 0);

    linglong::package::AppMetaInfoList pkgList1;
    result = getInstalledAppInfo("org.demo.test", "", "x86_64", userName, pkgList1);
    EXPECT_EQ(result, true);

    it = pkgList1.at(0);
    EXPECT_EQ(it->version, "8.3.9");

    ret = deleteAppRecord("org.demo.test", "", "x86_64", userName);
    EXPECT_EQ(ret, 0);
}