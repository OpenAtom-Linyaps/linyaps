/*
 * Copyright (c) 2022. ${ORGANIZATION_NAME}. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <QDir>
#include "project.h"

#include "module/repo/ostree.h"

#include "builder_config.h"
#include "depend_fetcher.h"
#include "project.h"

namespace linglong {
namespace builder {

class DependFetcherPrivate
{
public:
    explicit DependFetcherPrivate(const BuildDepend &bd)
        : ref(fuzzyRef(&bd))
    {
    }
    package::Ref ref;
    //    Project *project;
};

DependFetcher::DependFetcher(const BuildDepend &bd, Project *parent)
    : QObject(parent)
    , dd_ptr(new DependFetcherPrivate(bd))
{
    //    dd_ptr->project = parent;
}

DependFetcher::~DependFetcher() = default;

util::Error DependFetcher::fetch(const QString &subPath, const QString &targetPath)
{
    repo::OSTree ostree(BuilderConfig::instance()->repoPath());

    auto remoteRef = package::Ref("", dd_ptr->ref.appId, dd_ptr->ref.version, dd_ptr->ref.arch);

    auto ret = ostree.pull(remoteRef, true);
    if (!ret.success()) {
        return NewError(ret, -1, "pull " + remoteRef.toString() + " failed");
    }

    QDir targetParentDir(targetPath);
    targetParentDir.cdUp();
    targetParentDir.mkpath(".");

    ret = ostree.checkout(remoteRef, subPath, targetPath);

    return WrapError(ret, QString("ostree checkout %1 with subpath '%2' to %3")
                              .arg(remoteRef.toLocalRefString())
                              .arg(subPath)
                              .arg(targetPath));
}

} // namespace builder
} // namespace linglong