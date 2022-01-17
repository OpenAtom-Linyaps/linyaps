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
}

namespace repo {

// FIXME: move to class LocalRepo
package::Ref latestOfRef(const QString &appId, const QString &appVersion = "");

QString rootOfLayer(const package::Ref &ref);

class Repo
{
public:
    virtual util::Result import(const package::Bundle &bundle) = 0;

    virtual util::Result exportBundle(package::Bundle &bundle) = 0;

    virtual std::tuple<util::Result, QList<package::Ref>> list(const QString &filter) = 0;

    virtual std::tuple<util::Result, QList<package::Ref>> query(const QString &filter) = 0;

    virtual util::Result push(const package::Ref &ref, bool force) = 0;

    virtual util::Result push(const package::Bundle &bundle, bool force) = 0;

    virtual util::Result pull(const package::Ref &ref, bool force) = 0;
};

} // namespace repo
} // namespace linglong

#endif /* LINGLONG_BOX_SRC_MODULE_REPO_REPO_H_ */
