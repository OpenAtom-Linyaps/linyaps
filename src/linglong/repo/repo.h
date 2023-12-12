/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_MODULE_REPO_REPO_H_
#define LINGLONG_SRC_MODULE_REPO_REPO_H_

#include "linglong/package/package.h"
#include "linglong/utils/error/error.h"

#include <qt5/QtCore/qobjectdefs.h>

#include <QString>

namespace linglong {

namespace package {
class Bundle;
class Ref;
} // namespace package

namespace repo {

class Repo
{
public:
    Repo() = default;
    Repo(const Repo &) = delete;
    Repo(Repo &&) = delete;
    Repo &operator=(const Repo &) = delete;
    Repo &operator=(Repo &&) = delete;
    virtual ~Repo() = default;

    virtual linglong::utils::error::Result<void> importDirectory(const package::Ref &ref,
                                                                 const QString &path) = 0;

    virtual linglong::utils::error::Result<void> push(const package::Ref &ref) = 0;

    virtual linglong::utils::error::Result<void> pull(package::Ref &ref, bool force) = 0;
    virtual linglong::utils::error::Result<void> pullAll(const package::Ref &ref, bool force) = 0;
    virtual linglong::utils::error::Result<void> checkout(const package::Ref &ref,
                                                          const QString &subPath,
                                                          const QString &target) = 0;

    virtual linglong::utils::error::Result<void> checkoutAll(const package::Ref &ref,
                                                             const QString &subPath,
                                                             const QString &target) = 0;

    virtual QString rootOfLayer(const package::Ref &ref) = 0;

    virtual linglong::utils::error::Result<QString> remoteShowUrl(const QString &repoName) = 0;

    virtual linglong::utils::error::Result<package::Ref>
    localLatestRef(const package::Ref &ref) = 0;

    virtual package::Ref remoteLatestRef(const package::Ref &ref) = 0;

    virtual package::Ref latestOfRef(const QString &appId, const QString &appVersion) = 0;

    virtual linglong::utils::error::Result<void> getRemoteRepoList(const QString &repoPath,
                                                                   QVector<QString> &vec) = 0;
    virtual bool getRemoteRefs(const QString &repoPath,
                               const QString &remoteName,
                               QMap<QString, QString> &outRefs,
                               QString &err) = 0;
    virtual linglong::utils::error::Result<void> repoPullbyCmd(const QString &destPath,
                                                               const QString &remoteName,
                                                               const QString &ref) = 0;
    virtual linglong::utils::error::Result<void> repoDeleteDatabyRef(const QString &repoPath,
                                                                     const QString &ref) = 0;
};

} // namespace repo
} // namespace linglong

#endif /* LINGLONG_SRC_MODULE_REPO_REPO_H_ */
