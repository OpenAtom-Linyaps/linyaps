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

    virtual util::Error importDirectory(const package::Ref &ref, const QString &path);

    virtual util::Error import(const package::Bundle &bundle);

    virtual util::Error exportBundle(package::Bundle &bundle);

    virtual std::tuple<util::Error, QList<package::Ref>> list(const QString &filter);

    virtual std::tuple<util::Error, QList<package::Ref>> query(const QString &filter);

    virtual util::Error push(const package::Ref &ref, bool force);

    virtual util::Error push(const package::Ref &ref);

    virtual util::Error push(const package::Bundle &bundle, bool force);

    virtual util::Error pull(const package::Ref &ref, bool force);

    virtual QString rootOfLayer(const package::Ref &ref);

    virtual package::Ref latestOfRef(const QString &appId, const QString &appVersion);

private:
    QScopedPointer<VfsRepoPrivate> dd_ptr;
    Q_DECLARE_PRIVATE_D(qGetPtrHelper(dd_ptr), VfsRepo)
};

} // namespace repo
} // namespace linglong

#endif // LINGLONG_VFS_REPO_H
