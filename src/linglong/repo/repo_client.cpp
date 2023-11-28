/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "repo_client.h"

#include "linglong/package/package.h"
#include "linglong/util/config/config.h"
#include "linglong/util/error.h"
#include "linglong/util/file.h"
#include "linglong/util/http/http_client.h"
#include "linglong/util/qserializer/deprecated.h"
#include "linglong/utils/error/error.h"

#include <QEventLoop>
#include <QJsonObject>

#include <tuple>

namespace linglong {
namespace repo {

using namespace api::client;

QSERIALIZER_IMPL(Response);

QJsonObject getUserInfo()
{
    auto filePath = util::getUserFile(".linglong/.user.json");
    QJsonObject infoObj;

    QFile file(filePath);
    if (file.exists()) {
        file.open(QIODevice::ReadOnly);

        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        infoObj = doc.object();

        file.close();
    } else {
        QTextStream qin(stdin, QIODevice::ReadOnly);
        QString name;
        QString password;

        // FIXME: DO NOT log to console, should use tui output
        qInfo() << "please enter ldap account: ";
        qin >> name;
        qInfo() << "please enter password: ";
        qin >> password;
        infoObj["username"] = name;
        infoObj["password"] = password;
    }

    return infoObj;
}

linglong::utils::error::Result<QList<QSharedPointer<package::AppMetaInfo>>>
RepoClient::QueryApps(const package::Ref &ref)
{
    auto client = this->Client();
    client->deleteLater();
    Request_FuzzySearchReq req;
    req.setAppId(ref.appId);
    req.setVersion(ref.version);
    req.setArch(ref.arch);
    req.setRepoName(ref.repo);

    linglong::utils::error::Result<QList<QSharedPointer<package::AppMetaInfo>>> ret =
      LINGLONG_ERR(-1, "unknown error");

    QEventLoop loop;
    QEventLoop::connect(client,
                        &ClientApi::fuzzySearchAppSignal,
                        [&loop, &ret](FuzzySearchApp_200_response resp) {
                            loop.exit();
                            if (resp.getCode() != 0) {
                                ret = LINGLONG_ERR(resp.getCode(), resp.getMsg());
                                return;
                            }
                            QJsonObject obj = resp.asJsonObject();
                            QJsonDocument doc(obj);
                            auto bytes = doc.toJson();
                            auto respJson = util::loadJsonBytes<repo::Response>(bytes);
                            ret = respJson->data;
                            return;
                        });
    QEventLoop::connect(client,
                        &ClientApi::fuzzySearchAppSignalEFull,
                        [&loop, &ret](auto, auto error_type, const QString &error_str) {
                            loop.exit();
                            ret = LINGLONG_ERR(error_type, error_str);
                        });
    client->fuzzySearchApp(req);
    loop.exec();
    return ret;
}

ClientApi *RepoClient::Client()
{
    auto api = new ClientApi();
    auto manager = new QNetworkAccessManager(api);
    api->setNetworkAccessManager(manager);
    api->setNewServerForAllOperations(this->endpoint);
    return api;
}

RepoClient::RepoClient(const QString &endpoint)
    : endpoint(endpoint)
{
}

void RepoClient::setEndpoint(const QString &endpoint)
{
    this->endpoint = endpoint;
}

} // namespace repo
} // namespace linglong
