/*
 * Copyright (c) 2022. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LINGLONG_SRC_MODULE_REPO_OSTREE_H_
#define LINGLONG_SRC_MODULE_REPO_OSTREE_H_

#include <QObject>
#include <QScopedPointer>

#include "module/repo/repo.h"

namespace linglong {

namespace repo {

class OSTreeRepoPrivate;
class OSTreeRepo
    : public QObject
    , public Repo
{
    Q_OBJECT
public:
    enum Mode {
        Bare,
        BareUser,
        BareUserOnly,
        Archive,
    };
    Q_ENUM(Mode);

    explicit OSTreeRepo(const QString &path);

    ~OSTreeRepo() override;
    linglong::util::Error init(const QString &repoMode);

    linglong::util::Error remoteAdd(const QString &repoName, const QString &repoUrl);

    std::tuple<linglong::util::Error, QStringList> remoteList();

    linglong::util::Error importDirectory(const package::Ref &ref, const QString &path) override;

    linglong::util::Error import(const package::Bundle &bundle) override;

    linglong::util::Error exportBundle(package::Bundle &bundle) override;

    std::tuple<linglong::util::Error, QList<package::Ref>> list(const QString &filter) override;

    std::tuple<linglong::util::Error, QList<package::Ref>> query(const QString &filter) override;

    linglong::util::Error push(const package::Ref &ref, bool force) override;

    linglong::util::Error push(const package::Bundle &bundle, bool force) override;

    linglong::util::Error pull(const package::Ref &ref, bool force) override;

    linglong::util::Error checkout(const package::Ref &ref, const QString &subPath, const QString &target);

    linglong::util::Error removeRef(const package::Ref &ref);

    QString rootOfLayer(const package::Ref &ref) override;

    package::Ref latestOfRef(const QString &appId, const QString &appVersion) override;

private:
    QScopedPointer<OSTreeRepoPrivate> dd_ptr;
    Q_DECLARE_PRIVATE_D(qGetPtrHelper(dd_ptr), OSTreeRepo)
};

} // namespace repo
} // namespace linglong

#endif // LINGLONG_SRC_MODULE_REPO_OSTREE_H_
