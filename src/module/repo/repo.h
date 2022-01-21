/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LINGLONG_BOX_SRC_MODULE_REPO_REPO_H_
#define LINGLONG_BOX_SRC_MODULE_REPO_REPO_H_

#include <QString>

#include "module/util/result.h"

namespace linglong {

namespace package {
class Bundle;
class Ref;
} // namespace package

namespace repo {

extern const char *kRepoRoot;

class Repo
{
public:
    virtual util::Error importDirectory(const package::Ref &ref, const QString &path) = 0;

    virtual util::Error import(const package::Bundle &bundle) = 0;

    virtual util::Error exportBundle(package::Bundle &bundle) = 0;

    virtual std::tuple<util::Error, QList<package::Ref>> list(const QString &filter) = 0;

    virtual std::tuple<util::Error, QList<package::Ref>> query(const QString &filter) = 0;

    virtual util::Error push(const package::Ref &ref, bool force) = 0;

    virtual util::Error push(const package::Bundle &bundle, bool force) = 0;

    virtual util::Error pull(const package::Ref &ref, bool force) = 0;

    virtual QString rootOfLayer(const package::Ref &ref) = 0;

    virtual package::Ref latestOfRef(const QString &appId, const QString &appVersion) = 0;
};

} // namespace repo
} // namespace linglong

#endif /* LINGLONG_BOX_SRC_MODULE_REPO_REPO_H_ */
