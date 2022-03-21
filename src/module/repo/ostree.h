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

#include "repo.h"

namespace linglong {

namespace repo {

class OSTreePrivate;
class OSTree
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

    explicit OSTree(const QString &path);

    ~OSTree() override;
    util::Error init(const QString &repoMode);

    util::Error remoteAdd(const QString &repoName, const QString &repoUrl);

    util::Error importDirectory(const package::Ref &ref, const QString &path) override;

    util::Error import(const package::Bundle &bundle) override;

    util::Error exportBundle(package::Bundle &bundle) override;

    std::tuple<util::Error, QList<package::Ref>> list(const QString &filter) override;

    std::tuple<util::Error, QList<package::Ref>> query(const QString &filter) override;

    util::Error push(const package::Ref &ref, bool force) override;

    util::Error push(const package::Bundle &bundle, bool force) override;

    util::Error pull(const package::Ref &ref, bool force) override;

    util::Error checkout(const package::Ref &ref, const QString &subPath, const QString &target);

    QString rootOfLayer(const package::Ref &ref) override;

    package::Ref latestOfRef(const QString &appId, const QString &appVersion) override;

private:
    QScopedPointer<OSTreePrivate> dd_ptr;
    Q_DECLARE_PRIVATE_D(qGetPtrHelper(dd_ptr), OSTree)
};

} // namespace repo
} // namespace linglong

#endif // LINGLONG_SRC_MODULE_REPO_OSTREE_H_
