/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "depend_fetcher.h"
#include "depend_fetcher_p.h"

#include "builder_config.h"
#include "module/repo/ostree_repo.h"
#include "module/util/consoletext.h"

#include <QDir>

namespace linglong {
namespace builder {

DependFetcherPrivate::DependFetcherPrivate(const BuildDepend &bd, Project *parent)
    : ref(fuzzyRef(&bd))
    , project(parent)
    , buildDepend(&bd)
    , dependType(bd.type)
{
}

DependFetcher::DependFetcher(const BuildDepend &bd, Project *parent)
    : QObject(parent)
    , dd_ptr(new DependFetcherPrivate(bd, parent))
{
}

DependFetcher::~DependFetcher() = default;

linglong::util::Error DependFetcherPrivate::fetch(const QString &subPath, const QString &targetPath)
{
    // pull the package data which module is runtime
    ref.module = "runtime";
    auto ret = pullAndCheckout(subPath, targetPath);
    if (!ret.success())
        return ret;

    // pull the package data which module is devel
    ref.module = "devel";
    return pullAndCheckout(subPath, targetPath);
}

linglong::util::Error DependFetcherPrivate::pullAndCheckout(const QString &subPath,
                                                            const QString &targetPath)
{
    auto ret = NoError();
    repo::OSTreeRepo ostree(BuilderConfig::instance()->repoPath(),
                            BuilderConfig::instance()->remoteRepoEndpoint,
                            BuilderConfig::instance()->remoteRepoName);
    connect(&ostree,
            &repo::OSTreeRepo::progressChanged,
            this,
            &DependFetcherPrivate::printProgress);

    // depends with source > depends from remote > depends from local
    if (!buildDepend->source) {
        ref.repo = BuilderConfig::instance()->remoteRepoName;
        ref.channel = "linglong";

        // FIXME(black_desk):
        // 1. We should not use ostree repo in ll-builder, we should use the
        //    repo interface;
        // 2. Offline should not only be an option of builder, but also a work
        //    mode argument passed to repo, which prevent all network request.
        // 3. For now we just leave these code here, we will refactor them later.
        if (BuilderConfig::instance()->getOffline()) {
            ref = ostree.localLatestRef(ref);
        } else {
            ref = ostree.remoteLatestRef(ref);
            util::printReplacedText(QString("%1%2%3%4")
                                            .arg(ref.appId, -20)
                                            .arg(ref.version, -15)
                                            .arg(ref.module, -15)
                                            .arg("..."));

            ret = ostree.pull(ref, true);
            if (!ret.success()) {
                return WrapError(ret, "pull " + ref.toString() + " failed");
            }
        }
    }

    QDir targetParentDir(targetPath);
    targetParentDir.cdUp();
    targetParentDir.mkpath(".");
    util::printReplacedText(QString("%1%2%3%4")
                                    .arg(ref.appId, -20)
                                    .arg(ref.version, -15)
                                    .arg(ref.module, -15)
                                    .arg("checkout"));

    ret = ostree.checkout(ref, subPath, targetPath);

    if (!ret.success()) {
        return WrapError(ret, QString("ostree checkout %1 failed").arg(ref.toLocalRefString()));
    }

    util::printReplacedText(QString("%1%2%3%4")
                                    .arg(ref.appId, -20)
                                    .arg(ref.version, -15)
                                    .arg(ref.module, -15)
                                    .arg("complete\n"));

    // for app,lib. if the dependType match runtime, should be submitted together.
    if (dependType == DependTypeRuntime) {
        auto targetInstallPath = project->config().cacheAbsoluteFilePath(
                { "overlayfs", "up", project->config().targetInstallPath("") });

        ret = ostree.checkout(ref, subPath, targetInstallPath);
    }

    return WrapError(ret,
                     QString("ostree checkout %1 with subpath '%2' to %3")
                             .arg(ref.toLocalRefString())
                             .arg(subPath)
                             .arg(targetPath));
}

void DependFetcherPrivate::printProgress(const uint &progress, const QString &speed)
{
    util::printReplacedText(QString("%1%2%3%4 (%5\% %6/s)")
                                    .arg(ref.appId, -20)
                                    .arg(ref.version, -15)
                                    .arg(ref.module, -15)
                                    .arg("downloading")
                                    .arg(progress)
                                    .arg(speed));
}

linglong::util::Error DependFetcher::fetch(const QString &subPath, const QString &targetPath)
{
    Q_D(DependFetcher);

    return d->fetch(subPath, targetPath);
}

} // namespace builder
} // namespace linglong
