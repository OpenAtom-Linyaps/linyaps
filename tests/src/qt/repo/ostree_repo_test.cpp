/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "../util/mock-network.h"
#include "linglong/package/info.h"
#include "linglong/repo/ostree_repo.h"
#include "linglong/util/qserializer/json.h"

#include <qdir.h>
#include <qjsondocument.h>
#include <qtemporarydir.h>

#include <QTest>

using namespace linglong;
using namespace linglong::repo;
using namespace linglong::api::client;

class TestOSTreeRepo : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void push();
};

void TestOSTreeRepo::push()
{
    MockQNetworkAccessManager http;
    int requestStatusNumber = 0;
    connect(&http,
            &MockQNetworkAccessManager::onCreateRequest,
            [&requestStatusNumber](MockReply *rep, auto op, QNetworkRequest req) {
                auto url = req.url().toString();
                qDebug() << "onCreateRequest"
                         << "op" << op << "url" << url;
                // 查询仓库信息
                if (url.endsWith("/api/v1/repos/repo")) {
                    GetRepo_200_response resp;
                    resp.setCode(200);
                    rep->JSON(200, resp.asJsonObject());
                    return;
                }
                // 用户登陆
                if (url.endsWith("/api/v1/sign-in")) {
                    SignIn_200_response resp;
                    Response_SignIn data;
                    data.setToken("token");
                    resp.setData(data);
                    resp.setCode(200);
                    rep->JSON(200, resp.asJsonObject());
                    return;
                }
                if (!req.hasRawHeader(QByteArray("X-Token"))) {
                    rep->JSON(401, QJsonDocument());
                }
                auto taskID = QString("test_task_id");
                // 创建上传任务
                if (url.endsWith("/api/v1/upload-tasks")) {
                    NewUploadTaskID_200_response resp;
                    Response_NewUploadTaskResp data;
                    data.setId(taskID);
                    resp.setData(data);
                    resp.setCode(200);
                    rep->JSON(200, resp.asJsonObject());
                    return;
                }
                // 上传tar包
                if (url.endsWith(QString("/api/v1/upload-tasks/%1/tar").arg(taskID))) {
                    Api_UploadTaskFileResp resp;
                    Response_UploadTaskResp data;
                    data.setWatchId(0);
                    resp.setData(data);
                    resp.setCode(200);
                    rep->JSON(200, resp.asJsonObject());
                    return;
                }
                // 查询任务状态
                if (url.endsWith(QString("/api/v1/upload-tasks/%1/status").arg(taskID))) {
                    UploadTaskInfo_200_response resp;
                    Response_UploadTaskStatusInfo data;
                    if (requestStatusNumber < 3) {
                        data.setStatus("pending");
                    } else {
                        data.setStatus("complete");
                    }
                    requestStatusNumber++;
                    resp.setData(data);
                    resp.setCode(200);
                    rep->JSON(200, resp.asJsonObject());
                    return;
                }
                rep->JSON(500, QJsonDocument());
            });
    auto endpoint = QString("https://testmock.deepin.org");
    ClientApi api;
    api.setNewServerForAllOperations(endpoint);
    api.setNetworkAccessManager(&http);

    auto config = config::ConfigV1{ "repo", { { "repo", endpoint.toStdString() } }, 1 };

    // TODO(wurongjie) 不加 new 程序会挂掉
    linglong::util::Connection dbConnection("./testdata/ostree_repo_test.sqlite");
    auto repo = new OSTreeRepo("./testdata/ostree_repo_test", config, api, dbConnection);
    auto ref = package::Ref("test");

    // 创建一个临时目录
    QTemporaryDir d;
    QFile f(d.filePath("info.json"));
    QSharedPointer<package::Info> info(new package::Info);
    info->appid = "org.deepin.test";
    info->arch = QStringList{ "x86" };
    info->version = "1.0.0";
    info->module = "runtime";
    auto [data, err] = util::toJSON(info);
    if (err) {
        QVERIFY2(false, qPrintable("generate app info err " + err.message()));
    }
    QVERIFY2(f.open(QIODevice::WriteOnly | QIODevice::Text), "failed to open file");
    QVERIFY2(f.write(data), "failded to write file");
    f.close();

    // 将目录做为包导入到ostree
    auto importResult = repo->importDirectory(ref, d.path());
    if (!importResult.has_value()) {
        QVERIFY2(false, qPrintable("push error: " + importResult.error().message()));
    }
    auto pushResult = repo->push(ref);
    if (!pushResult.has_value()) {
        QVERIFY2(false, qPrintable("push error: " + pushResult.error().message()));
    }
}

QTEST_GUILESS_MAIN(TestOSTreeRepo)
#include "ostree_repo_test.moc"
