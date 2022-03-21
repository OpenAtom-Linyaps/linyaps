/*
 * Copyright (c) 2021. Uniontech Software Ltd. All rights reserved.
 *
 * Author:     Iceyer <me@iceyer.net>
 *
 * Maintainer: Iceyer <me@iceyer.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "ostree.h"

#include <QProcess>
#include <QDir>

#include "module/package/ref.h"
#include "module/util/runner.h"
#include "module/util/sysinfo.h"

namespace linglong {
namespace repo {

class OSTreePrivate
{
    OSTreePrivate(const QString &path, OSTree *parent)
        : path(path)
        , q_ptr(parent)
    {
        // FIXME: should be repo
        if (QDir(path).exists("/repo/repo")) {
            qCritical() << path;
            ostreePath = path + "/repo/repo";
        } else {
            ostreePath = path + "/repo";
        }
    }

    util::Error ostreeRun(const QStringList &args, QByteArray *stdout = nullptr)
    {
        QProcess ostree;
        ostree.setProgram("ostree");

        QStringList ostreeArgs = {"-v", "--repo=" + ostreePath};
        ostreeArgs.append(args);
        ostree.setArguments(ostreeArgs);

        QProcess::connect(&ostree, &QProcess::readyReadStandardOutput, [&]() {
            qDebug() << QString::fromLocal8Bit(ostree.readAllStandardOutput());
            if (stdout) {
                *stdout += ostree.readAllStandardOutput();
            }
        });

        QProcess::connect(&ostree, &QProcess::readyReadStandardError,
                          [&]() { qDebug() << QString::fromLocal8Bit(ostree.readAllStandardError()); });

        qDebug() << "start" << ostree.arguments().join(" ");
        ostree.start();
        ostree.waitForFinished(-1);
        qDebug() << ostree.exitStatus() << "with exit code:" << ostree.exitCode();

        return NewError(ostree.exitCode(), ostree.errorString());
    }

    QString path;
    QString ostreePath;

    OSTree *q_ptr;
    Q_DECLARE_PUBLIC(OSTree);
};

linglong::util::Error OSTree::importDirectory(const package::Ref &ref, const QString &path)
{
    Q_D(OSTree);

    return NewError(d->ostreeRun({"commit", "-b", ref.toString(), "--tree=dir=" + path}));
}

util::Error OSTree::import(const package::Bundle &bundle)
{
    return util::Error(nullptr, 0, nullptr);
}

util::Error OSTree::exportBundle(package::Bundle &bundle)
{
    return util::Error(nullptr, 0, nullptr);
}

std::tuple<util::Error, QList<package::Ref>> OSTree::list(const QString &filter)
{
    return std::tuple<util::Error, QList<package::Ref>>(NewError(), {});
}

std::tuple<util::Error, QList<package::Ref>> OSTree::query(const QString &filter)
{
    return std::tuple<util::Error, QList<package::Ref>>(NewError(), {});
}

/*!
 * ostree-upload push --repo=/deepin/linglong/repo/repo
--token="eEQQSuJBRxFcwa0bz2O5u12w/g1cWXKzexcZGH599y2uYbHbC/IBnExe7KJuWhtBx364AJE0TcUiAIzCIYpQzA==" --address=http:
//bg.iceyer.net:18080 org.deepin.music/6.0.1.54/x86_64
 * @param ref
 * @param force
 * @return
 */
util::Error OSTree::push(const package::Ref &ref, bool force)
{
    Q_D(OSTree);

    auto serverUri = qEnvironmentVariable("OSTREE_UPLOAD_SERVER");
    auto serverToken = qEnvironmentVariable("OSTREE_UPLOAD_TOKEN");

    auto success = runner::Runner("ostree-upload", {
                                                       "push",
                                                       "--repo=" + d->ostreePath,
                                                       // FIXME: where is the token, for user home
                                                       "--token=" + serverToken,
                                                       "--address=" + serverUri,
                                                       ref.toString(),
                                                   });

    return (success ? NoError() : NewError(-1, "ostree upload failed"));
}

util::Error OSTree::push(const package::Bundle &bundle, bool force)
{
    return util::Error(nullptr, 0, nullptr);
}

util::Error OSTree::pull(const package::Ref &ref, bool force)
{
    Q_D(OSTree);
    //Fixme: remote name maybe not repo and there should support multiple remote
    return WrapError(d->ostreeRun({"pull", "repo", "--mirror", ref.toString()}));
}

util::Error OSTree::init(const QString &mode)
{
    Q_D(OSTree);

    return WrapError(d->ostreeRun({"init", mode}));
}

util::Error OSTree::remoteAdd(const QString &repoName, const QString &repoUrl)
{
    Q_D(OSTree);

    return WrapError(d->ostreeRun({"remote", "add", "--no-gpg-verify", repoName, repoUrl}));
}

OSTree::OSTree(const QString &path)
    : dd_ptr(new OSTreePrivate(path, this))
{
}

util::Error OSTree::checkout(const package::Ref &ref, const QString &subPath, const QString &target)
{
    QStringList args = {"checkout", "--union"};
    if (!subPath.isEmpty()) {
        args.push_back("--subpath=" + subPath);
    }
    args.append({ref.toString(), target});
    return WrapError(dd_ptr->ostreeRun(args));
}

QString OSTree::rootOfLayer(const package::Ref &ref)
{
    return QStringList {dd_ptr->path, "layers", ref.appId, ref.version, ref.arch}.join(QDir::separator());
}

package::Ref OSTree::latestOfRef(const QString &appId, const QString &appVersion)
{
    auto latestVersionOf = [this](const QString &appId) {
        auto localRepoRoot = QString(dd_ptr->path) + "/layers" + "/" + appId;

        QDir appRoot(localRepoRoot);

        // found latest
        if (appRoot.exists("latest")) {
            return appRoot.absoluteFilePath("latest");
        }

        // FIXME: found biggest version
        appRoot.setSorting(QDir::Name | QDir::Reversed);
        auto verDirs = appRoot.entryList(QDir::NoDotAndDotDot | QDir::Dirs);
        auto available = verDirs.value(0);
        qDebug() << "available version" << available << appRoot << verDirs;
        return available;
    };

    // 未指定版本使用最新版本，指定版本下使用指定版本
    auto version = latestVersionOf(appId);
    if (!appVersion.isEmpty()) {
        version = appVersion;
    }
    auto ref = appId + "/" + version + "/" + util::hostArch();
    return package::Ref(ref);
}

OSTree::~OSTree() = default;

} // namespace repo
} // namespace linglong
