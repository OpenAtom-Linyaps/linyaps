/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "repo_client.h"

#include "linglong/package/package.h"
#include "linglong/util/config/config.h"
#include "linglong/util/file.h"
#include "linglong/util/http/http_client.h"
#include "linglong/util/qserializer/deprecated.h"

#include <QJsonObject>

namespace linglong {
namespace repo {

QSERIALIZER_IMPL(Response);
QSERIALIZER_IMPL(AuthResponseData);
QSERIALIZER_IMPL(AuthResponse);

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

std::tuple<util::Error, QList<QSharedPointer<package::AppMetaInfo>>>
RepoClient::QueryApps(const package::Ref &ref)
{
    // TODO: query cache Here
    QUrl url(endpoint);
    // FIXME: normalize the path
    url.setPath(url.path() + "/api/v0/apps/fuzzysearchapp");
    QNetworkRequest request(url);

    QJsonObject obj;
    obj["AppId"] = ref.appId;
    obj["version"] = ref.version;
    obj["arch"] = ref.arch;
    obj["repoName"] = ref.repo;

    QJsonDocument doc(obj);
    QByteArray data = doc.toJson();

    util::HttpRestClient hc;
    auto reply = hc.post(request, data);
    if (reply->error()) {
        return { NewError(-1, reply->errorString()), {} };
    }
    data = reply->readAll();
    qDebug() << "QueryApps: get response from server:" << QString(data);
    auto resp = util::loadJsonBytes<repo::Response>(data);

    if (!resp) {
        return { NewError(-1, "Failed to load application list from response."), {} };
    }

    return { Success(), resp->data };
}

std::tuple<util::Error, QString> RepoClient::Auth(const package::Ref &ref)
{
    //    QUrl url(QString("%1/%2").arg(endpoint, "api/v1/sign-in"));
    QUrl url(QString("%1/%2").arg(endpoint, "auth"));

    QNetworkRequest request(url);

    auto userInfo = getUserInfo();

    util::HttpRestClient hc;
    auto reply = hc.post(request, QJsonDocument(userInfo).toJson());
    auto data = reply->readAll();
    auto result = util::loadJsonBytes<AuthResponse>(data);

    // TODO: use status macro for code 200
    if (200 != result->code) {
        return { NewError(-1, QString("getToken failed %1 %2").arg(result->code).arg(result->msg)),
                 "" };
    }

    return { Success(), result->data->token };
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
