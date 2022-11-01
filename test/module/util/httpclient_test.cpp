/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     huqinghong <huqinghong@uniontech.com>
 *
 * Maintainer: huqinghong <huqinghong@uniontech.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include <QtConcurrent/QtConcurrent>
#include "src/module/util/httpclient.h"
#include "src/module/util/file.h"

TEST(httpclient, test01)
{
    QStringList userInfo = {"linglong", "linglong"};

    QString configUrl = "";
    int statusCode = linglong::util::getLocalConfig("appDbUrl", configUrl);

    EXPECT_EQ (statusCode, STATUS_CODE(kSuccess));

    auto token = G_HTTPCLIENT->getToken(configUrl, userInfo);
    EXPECT_EQ (token.size() > 0, true);

    QtConcurrent::run([=]() {
        QString outMsg;
        G_HTTPCLIENT->queryRemoteApp("cal", "", "x86_64", outMsg);
        EXPECT_EQ (outMsg.size() > 0, true);
    });
}