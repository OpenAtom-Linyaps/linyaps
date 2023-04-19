/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_MODULE_REPO_REPO_CLIENT_H_
#define LINGLONG_SRC_MODULE_REPO_REPO_CLIENT_H_

#include "module/package/package.h"
#include "module/package/ref.h"
#include "module/util/error.h"

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

class RepoClient
{
public:
    //    RepoClient(const QString& repoPath);
    std::tuple<util::Error, QList<QSharedPointer<package::AppMetaInfo>>>
    QueryApps(const package::Ref &ref);

private:
};

} // namespace repo
} // namespace linglong

Q_JSON_DECLARE_PTR_METATYPE_NM(linglong::repo, Response)

#endif // LINGLONG_SRC_MODULE_REPO_REPO_CLIENT_H_
