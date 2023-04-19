/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <gtest/gtest.h>

#include "module/util/http/http_client.h"

#include <QtConcurrent/QtConcurrent>

TEST(Util, HttpClient)
{
    if (!qEnvironmentVariableIsSet("LINGLONG_TEST_ALL")) {
        return;
    }

    int argc = 0;
    char *argv = nullptr;
    QCoreApplication app(argc, &argv);

    QString endpoint = "https://linglong.dev";

    auto ret = QtConcurrent::run([&]() {
        linglong::util::HttpRestClient hc;
        QNetworkRequest req(endpoint);
        auto reply = hc.get(req);
        auto data = reply->readAll();

        EXPECT_EQ(data.size() > 0, true);
        QCoreApplication::exit(0);
    });

    QCoreApplication::exec();
}
