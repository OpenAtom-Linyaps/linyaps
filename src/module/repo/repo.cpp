/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "repo.h"

#include <QString>
#include <QDir>

#include "module/util/sysinfo.h"
#include "module/package/info.h"

namespace linglong {
namespace repo {

const auto kLayersRoot = "/deepin/linglong/layers";

// FIXME: move to class LocalRepo
package::Ref latestOf(const QString &appID)
{
    auto latestVersionOf = [](const QString &appID) {
        auto localRepoRoot = QString(kLayersRoot) + "/" + appID;

        QDir appRoot(localRepoRoot);

        // found latest
        if (appRoot.exists("latest")) {
            return appRoot.absoluteFilePath("latest");
        }

        auto verDirs = appRoot.entryList(QDir::NoDotAndDotDot | QDir::Dirs);

        // FIXME: found biggest version
        auto available = verDirs.value(0);
        qCritical() << "available version" << available << appRoot << verDirs;
        return available;
    };

    auto refID = appID + "/" + latestVersionOf(appID) + "/" + util::hostArch();
    return package::Ref(refID);
}

QString rootOfLayer(const package::Ref &ref)
{
    return QString(kLayersRoot) + "/" + ref.id + "/" + ref.version + "/" + ref.arch;
}

} // namespace repo
} // namespace linglong
