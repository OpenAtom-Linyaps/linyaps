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
package::Ref latestOf(const QString &appID, const QString &appVersion)
{
    auto latestVersionOf = [](const QString &appID) {
        auto localRepoRoot = QString(kLayersRoot) + "/" + appID;

        QDir appRoot(localRepoRoot);

        // found latest
        if (appRoot.exists("latest")) {
            return appRoot.absoluteFilePath("latest");
        }

        // FIXME: found biggest version
        appRoot.setSorting(QDir::Name | QDir::Reversed);
        auto verDirs = appRoot.entryList(QDir::NoDotAndDotDot | QDir::Dirs);
        auto available = verDirs.value(0);
        qInfo() << "available version" << available << appRoot << verDirs;
        return available;
    };

    // 未指定版本使用最新版本，指定版本下使用指定版本
    auto version = latestVersionOf(appID);
    if (!appVersion.isEmpty()) {
        version = appVersion;
    }
    auto ref = appID + "/" + version + "/" + util::hostArch();
    return package::Ref(ref);
}

QString rootOfLayer(const package::Ref &ref)
{
    return QString(kLayersRoot) + "/" + ref.id + "/" + ref.version + "/" + ref.arch;
}

} // namespace repo
} // namespace linglong
