/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_MODULE_REPO_REPO_CLIENT_H_
#define LINGLONG_SRC_MODULE_REPO_REPO_CLIENT_H_

#include "ClientApi.h"
#include "linglong/package/ref.h"
#include "linglong/repo/repo.h"
#include "linglong/utils/error/error.h"

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
    Q_JSON_PROPERTY(QList<QSharedPointer<linglong::package::Info>>, data);
};

class RepoClient : public QObject
{
public:
    explicit RepoClient(api::client::ClientApi &api, QObject *parent = nullptr);

    // FIXME(black_desk):
    // This method is just a workaround used to
    // update endpoint when endpoint get updated
    // by PackageManager::RepoModify.
    // It's not thread-safe.
    void setEndpoint(const QString &endpoint);

    linglong::utils::error::Result<QList<QSharedPointer<package::Info>>>
    QueryApps(const package::Ref &ref);

private:
    api::client::ClientApi &client;
};

} // namespace repo
} // namespace linglong

Q_JSON_DECLARE_PTR_METATYPE_NM(linglong::repo, Response)
#endif // LINGLONG_SRC_MODULE_REPO_REPO_CLIENT_H_
