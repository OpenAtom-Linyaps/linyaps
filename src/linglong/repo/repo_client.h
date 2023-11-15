/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_MODULE_REPO_REPO_CLIENT_H_
#define LINGLONG_SRC_MODULE_REPO_REPO_CLIENT_H_

#include "linglong/package/ref.h"
#include "linglong/repo/repo.h"
#include "linglong/utils/error/error.h"

#include "ClientApi.h"

#include <QNetworkReply>
#include <QNetworkRequest>

#include <tuple>

namespace linglong {
namespace repo {

class Response : public JsonSerialize
{
    Q_OBJECT;
    Q_JSON_CONSTRUCTOR(Response)
public:
    Q_JSON_PROPERTY(int, code);
    Q_JSON_PROPERTY(QList<QSharedPointer<linglong::package::AppMetaInfo>>, data);
};

class AuthResponseData : public Serialize
{
    Q_OBJECT;
    Q_JSON_CONSTRUCTOR(AuthResponseData)

    Q_JSON_PROPERTY(QString, token);
};

class AuthResponse : public Serialize
{
    Q_OBJECT;
    Q_JSON_CONSTRUCTOR(AuthResponse)

    Q_JSON_PROPERTY(int, code);
    Q_JSON_PTR_PROPERTY(linglong::repo::AuthResponseData, data);
    Q_JSON_PROPERTY(QString, msg);
};

class RepoClient : public QObject
{
public:
    explicit RepoClient(const QString &endpoint);

    // FIXME(black_desk):
    // This method is just a workaround used to
    // update RepoClient::endpoint
    // when endpoint get updated
    // by PackageManager::RepoModify.
    // It's not thread-safe.
    void setEndpoint(const QString &endpoint);

    linglong::utils::error::Result<QList<QSharedPointer<package::AppMetaInfo>>>
    QueryApps(const package::Ref &ref);

    linglong::utils::error::Result<QString> Auth(const package::Ref &ref);

    linglong::api::client::ClientApi *Client();

private:
    QString endpoint;
};

} // namespace repo
} // namespace linglong

Q_JSON_DECLARE_PTR_METATYPE_NM(linglong::repo, Response)
Q_JSON_DECLARE_PTR_METATYPE_NM(linglong::repo, AuthResponseData)
Q_JSON_DECLARE_PTR_METATYPE_NM(linglong::repo, AuthResponse)

#endif // LINGLONG_SRC_MODULE_REPO_REPO_CLIENT_H_
