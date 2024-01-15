/*
 * SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "linglong/builder/depend_fetcher.h"

#include "builder_config.h"
#include "linglong/cli/printer.h"
#include "linglong/repo/ostree_repo.h"
#include "linglong/util/error.h"
#include "project.h"

#include <QDir>

namespace linglong {
namespace builder {

class DependFetcherPrivate
{
public:
    explicit DependFetcherPrivate(QSharedPointer<const BuildDepend> bd, Project *parent)
        : ref(fuzzyRef(bd))
        , project(parent)
        , buildDepend(bd)
        , dependType(bd->type)
    {
    }

    // TODO: dependType should be removed, buildDepend include it
    package::Ref ref;
    Project *project;
    QSharedPointer<const BuildDepend> buildDepend;
    QString dependType;
};

DependFetcher::DependFetcher(QSharedPointer<const BuildDepend> bd,
                             repo::OSTreeRepo &repo,
                             cli::Printer &p,
                             Project *parent)
    : QObject(parent)
    , ref(fuzzyRef(bd))
    , ostree(repo)
    , printer(p)
    , dd_ptr(new DependFetcherPrivate(bd, parent))
{
    connect(&ostree, &repo::OSTreeRepo::progressChanged, this, &DependFetcher::printProgress);
}

DependFetcher::~DependFetcher() = default;

void DependFetcher::printProgress(const uint &progress, const QString &speed)
{
    printer.printReplacedText(QString("%1%2%3%4 (%5\% %6/s)")
                                    .arg(ref.appId, -20)
                                    .arg(ref.version, -15)
                                    .arg(ref.module, -15)
                                    .arg("downloading")
                                    .arg(progress)
                                    .arg(speed));
}

linglong::util::Error DependFetcher::fetch(const QString &subPath, const QString &targetPath)
{
    // depends from remote > depends from local
    ref.repo = BuilderConfig::instance()->remoteRepoName;
    ref.channel = "linglong";

    // FIXME(black_desk):
    // 1. Offline should not only be an option of builder, but also a work
    //    mode argument passed to repo, which prevent all network request.
    // 2. For now we just leave these code here, we will refactor them later.
    if (BuilderConfig::instance()->getOffline()) {
        ref = *ostree.localLatestRef(ref);

        printer.printMessage(QString("offline dependency: %1 %2").arg(ref.appId).arg(ref.version));
    } else {
        ref = ostree.remoteLatestRef(ref);
        printer.printReplacedText(QString("%1%2%3%4")
                                    .arg(ref.appId, -20)
                                    .arg(ref.version, -15)
                                    .arg(ref.module, -15)
                                    .arg("..."));
        auto ret = ostree.pullAll(ref, true);
        if (!ret.has_value()) {
            return WrapError(NewError(ret.error().code(), ret.error().message()),
                             "pull " + ref.toString() + " failed");
        }
    }

    QDir targetParentDir(targetPath);
    targetParentDir.cdUp();
    targetParentDir.mkpath(".");
    {
        printer.printReplacedText(QString("%1%2%3%4")
                                    .arg(ref.appId, -20)
                                    .arg(ref.version, -15)
                                    .arg(ref.module, -15)
                                    .arg("checkout"));
        auto ret = ostree.checkoutAll(ref, subPath, targetPath);
        if (!ret.has_value()) {
            return WrapError(NewError(ret.error().code(), ret.error().message()),
                             QString("ostree checkout %1 failed").arg(ref.toLocalRefString()));
        }

        printer.printReplacedText(QString("%1%2%3%4")
                                    .arg(ref.appId, -20)
                                    .arg(ref.version, -15)
                                    .arg(ref.module, -15)
                                    .arg("complete\n"));
    }
    // for app,lib. if the dependType match runtime, should be submitted together.
    if (dd_ptr->dependType == DependTypeRuntime) {
        auto targetInstallPath = dd_ptr->project->config().cacheAbsoluteFilePath(
          { "overlayfs", "up", dd_ptr->project->config().targetInstallPath("") });
        {
            auto ret = ostree.checkoutAll(ref, subPath, targetInstallPath);
            if (!ret.has_value()) {
                return WrapError(NewError(ret.error().code(), ret.error().message()),
                                 QString("ostree checkout %1 with subpath '%2' to %3")
                                   .arg(ref.toLocalRefString())
                                   .arg(subPath)
                                   .arg(targetPath));
            }
        }
    }
    return {};
}

} // namespace builder
} // namespace linglong