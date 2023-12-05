/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "../util/mock-network.h"
#include "linglong/repo/repo_client.h"

#include <QTest>
using namespace ::linglong;
using namespace linglong::api::client;

class TestRepoClient : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void NetworkReply();
    void QueryApps();
};

void TestRepoClient::NetworkReply()
{
    MockQNetworkAccessManager http;
    connect(&http, &MockQNetworkAccessManager::onCreateRequest, [](MockReply *req) {
        req->Bytes(400, "");
    });

    ClientApi api;
    api.setNewServerForAllOperations(QUrl("https://testmock.deepin.org"));
    api.setNetworkAccessManager(&http);

    auto client = repo::RepoClient(api);
    auto apps = client.QueryApps(package::Ref(""));
    QVERIFY(!apps.has_value());
    QVERIFY(apps.error().code() == 400);
}

void TestRepoClient::QueryApps()
{
    MockQNetworkAccessManager http;
    connect(&http, &MockQNetworkAccessManager::onCreateRequest, [](MockReply *req) {
        // 构造响应内容
        Request_RegisterStruct reg;
        reg.setAppId("org.deepin.music");
        FuzzySearchApp_200_response data;
        data.setCode(200);
        data.setData({ reg });
        req->JSON(200, data.asJsonObject());
    });

    ClientApi api;
    api.setNewServerForAllOperations(QUrl("https://testmock.deepin.org"));
    api.setNetworkAccessManager(&http);

    //  构造请求内容
    auto appID = "org.deepin.music";
    auto client = repo::RepoClient(api);
    auto ref = package::Ref("", appID, "", "x86_64");

    {
        auto apps = client.QueryApps(ref);
        if (!apps.has_value()) {
            qCritical() << apps.error().message();
        }
        QVERIFY(apps.has_value());
        auto exists = false;
        for (const auto &app : *apps) {
            if (app->appId == appID) {
                exists = true;
            }
        }
        QVERIFY(exists);
    }
}

QTEST_GUILESS_MAIN(TestRepoClient)
#include "repo_client_test.moc"
