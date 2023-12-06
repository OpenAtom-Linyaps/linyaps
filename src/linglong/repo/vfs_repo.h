/*
 * SPDX-FileCopyrightText: 2023 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_MODULE_REPO_VFS_REPO_H_
#define LINGLONG_SRC_MODULE_REPO_VFS_REPO_H_

#include "linglong/package/package.h"
#include "linglong/repo/repo.h"

#include <QPointer>
#include <QScopedPointer>

namespace linglong {
namespace repo {

class VfsRepoPrivate;

class VfsRepo : public QObject, public Repo
{
    Q_OBJECT
public:
    explicit VfsRepo(const QString &path);

    ~VfsRepo() override;

    linglong::utils::error::Result<void> importDirectory(const package::Ref &ref,
                                                         const QString &path) override;

    linglong::utils::error::Result<void> push(const package::Ref &ref, bool force) override;

    linglong::utils::error::Result<void> push(const package::Ref &ref) override;

    linglong::utils::error::Result<void> pull(package::Ref &ref, bool force) override;
    linglong::utils::error::Result<void> pull(const QString &ref) override;
    QString rootOfLayer(const package::Ref &ref) override;

    package::Ref latestOfRef(const QString &appId, const QString &appVersion) override;

    linglong::utils::error::Result<void> remoteAdd(const QString &repoName,
                                                   const QString &repoUrl) override;

    linglong::utils::error::Result<void> pullAll(const package::Ref &ref, bool force) override;
    linglong::utils::error::Result<void> checkout(const package::Ref &ref,
                                                  const QString &subPath,
                                                  const QString &target) override;

    linglong::utils::error::Result<void> remoteDelete(const QString &repoName) override;
    linglong::utils::error::Result<void> checkoutAll(const package::Ref &ref,
                                                     const QString &subPath,
                                                     const QString &target) override;

    linglong::utils::error::Result<QString> remoteShowUrl(const QString &repoName) override;

    linglong::utils::error::Result<package::Ref> localLatestRef(const package::Ref &ref) override;

    package::Ref remoteLatestRef(const package::Ref &ref) override;

    linglong::utils::error::Result<void> getRemoteRepoList(const QString &repoPath,
                                                           QVector<QString> &vec) override;
    bool getRemoteRefs(const QString &repoPath,
                       const QString &remoteName,
                       QMap<QString, QString> &outRefs,
                       QString &err) override;
    linglong::utils::error::Result<void> checkOutAppData(const QString &repoPath,
                                                         const QString &remoteName,
                                                         const QString &ref,
                                                         const QString &dstPath) override;
    linglong::utils::error::Result<void> repoPullbyCmd(const QString &destPath,
                                                       const QString &remoteName,
                                                       const QString &ref) override;
    linglong::utils::error::Result<void> repoDeleteDatabyRef(const QString &repoPath,
                                                             const QString &ref) override;

private:
    QScopedPointer<VfsRepoPrivate> dd_ptr;
    Q_DECLARE_PRIVATE_D(qGetPtrHelper(dd_ptr), VfsRepo)
};

} // namespace repo
} // namespace linglong

#endif // LINGLONG_VFS_REPO_H
