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

    linglong::utils::error::Result<void> import(const package::Bundle &bundle) override;

    linglong::utils::error::Result<void> exportBundle(package::Bundle &bundle) override;

    linglong::utils::error::Result<QList<package::Ref>> list(const QString &filter) override;

    linglong::utils::error::Result<QList<package::Ref>> query(const QString &filter) override;

    linglong::utils::error::Result<void> push(const package::Ref &ref, bool force) override;

    linglong::utils::error::Result<void> push(const package::Ref &ref) override;

    linglong::utils::error::Result<void> push(const package::Bundle &bundle, bool force) override;

    linglong::utils::error::Result<void> pull(const package::Ref &ref, bool force) override;

    QString rootOfLayer(const package::Ref &ref) override;

    package::Ref latestOfRef(const QString &appId, const QString &appVersion) override;

private:
    QScopedPointer<VfsRepoPrivate> dd_ptr;
    Q_DECLARE_PRIVATE_D(qGetPtrHelper(dd_ptr), VfsRepo)
};

} // namespace repo
} // namespace linglong

#endif // LINGLONG_VFS_REPO_H
