/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include <QtConcurrent/QtConcurrent>

#include "src/module/util/file.h"
#include "src/module/util/http/httpclient.h"

TEST(httpclient, test01)
{
    QStringList userInfo = {"linglong", "linglong"};

    QString configUrl = "";
    int statusCode = linglong::util::getLocalConfig("appDbUrl", configUrl);

    EXPECT_EQ(statusCode, STATUS_CODE(kSuccess));

    auto token = HTTPCLIENT->getToken(configUrl, userInfo);
    EXPECT_EQ(token.size() > 0, true);

    QtConcurrent::run([=]() {
        QString outMsg;
        HTTPCLIENT->queryRemoteApp("repo", "cal", "", "x86_64", outMsg);
        EXPECT_EQ(outMsg.size() > 0, true);
    });
}