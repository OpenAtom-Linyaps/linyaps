/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "module/package/ref.h"
#include "module/util/config/config.h"
#include "module/util/file.h"
#include "module/util/http/httpclient.h"

#include <QtConcurrent/QtConcurrent>

// TODO: test with mock data
TEST(Util, HttpClient)
{
    if (!qEnvironmentVariableIsSet("LINGLONG_TEST_ALL")) {
        return;
    }

    int argc = 0;
    char *argv = nullptr;
    QCoreApplication app(argc, &argv);

    QStringList userInfo = { "linglong", "linglong" };

    QString configUrl = ConfigInstance().repos[linglong::package::kDefaultRepo]->endpoint;

    EXPECT_EQ(true, !configUrl.isEmpty());

    //    auto token = HTTPCLIENT->getToken(configUrl, userInfo);
    //    EXPECT_EQ(token.size() > 0, true);

    auto ret = QtConcurrent::run([&]() {
        QString outMsg;
        HTTPCLIENT->queryRemoteApp("cal", "", "1.0.0", "x86_64", outMsg);
        //        EXPECT_EQ(outMsg.size() > 0, true);
        QCoreApplication::exit(0);
    });

    QCoreApplication::exec();
}
